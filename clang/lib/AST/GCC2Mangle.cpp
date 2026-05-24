//===------- GCC2Mangle.cpp - Mangle C++ Names for Legacy GCC 2.x ABI ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides C++ name mangling targeting the legacy GCC 2.x C++ ABI.
//
//===----------------------------------------------------------------------===//

#include "CXXABI.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include "clang/AST/RecursiveASTVisitor.h"

using namespace clang;

namespace {

class LocalTagVisitor : public RecursiveASTVisitor<LocalTagVisitor> {
  const FunctionDecl *FD;
  std::vector<const TagDecl *> &Tags;
public:
  LocalTagVisitor(const FunctionDecl *FD, std::vector<const TagDecl *> &Tags)
      : FD(FD), Tags(Tags) {}

  bool VisitTagDecl(TagDecl *TD) {
    if (TD->getParentFunctionOrMethod() == FD) {
      if (llvm::find(Tags, TD) == Tags.end()) {
        Tags.push_back(TD);
      }
    }
    return true;
  }
};

static CharUnits getBaseOffset(const ASTContext &Context, const CXXRecordDecl *Derived, const CXXRecordDecl *Base) {
  if (Derived->getCanonicalDecl() == Base->getCanonicalDecl())
    return CharUnits::Zero();

  CXXBasePaths Paths;
  if (!Derived->isDerivedFrom(Base, Paths))
    return CharUnits::Zero();

  const CXXBasePath &Path = Paths.front();
  CharUnits Offset = CharUnits::Zero();
  const CXXRecordDecl *MostDerived = Derived;
  for (unsigned I = 0, N = Path.size(); I != N; ++I) {
    const CXXBasePathElement &Element = Path[I];
    const CXXRecordDecl *Parent = Element.Class;
    const CXXRecordDecl *Child = Element.Base->getType()->getAsCXXRecordDecl();
    
    if (Element.Base->isVirtual()) {
      const ASTRecordLayout &MostDerivedLayout = Context.getASTRecordLayout(MostDerived);
      Offset = MostDerivedLayout.getVBaseClassOffset(Child);
    } else {
      const ASTRecordLayout &ParentLayout = Context.getASTRecordLayout(Parent);
      Offset += ParentLayout.getBaseClassOffset(Child);
    }
  }
  return Offset;
}

static bool isIgnoredStdNamespace(const DeclContext *DC) {
  while (DC && isa<LinkageSpecDecl>(DC))
    DC = DC->getParent();
    
  const auto *ND = dyn_cast_or_null<NamespaceDecl>(DC);
  if (!ND) {
    return false;
  }
  
  if (!(ND->isStdNamespace() || (ND->getIdentifier() && ND->getIdentifier()->isStr("std")))) {
    return false;
  }
  
  const DeclContext *Parent = ND->getParent();
  while (Parent && isa<LinkageSpecDecl>(Parent))
    Parent = Parent->getParent();
    
  if (Parent->isTranslationUnit()) {
    return true;
  }
  
  return isIgnoredStdNamespace(Parent);
}

static bool isEffectivelyTranslationUnit(const DeclContext *DC) {
  while (DC && !DC->isTranslationUnit()) {
    if (isIgnoredStdNamespace(DC)) {
      DC = DC->getParent();
      continue;
    }
    if (isa<LinkageSpecDecl>(DC)) {
      DC = DC->getParent();
      continue;
    }
    return false;
  }
  return true;
}

class GCC2MangleContextImpl : public GCC2MangleContext {
  std::unique_ptr<ItaniumMangleContext> Fallback;
  llvm::DenseMap<const NamedDecl *, unsigned> Uniquifier;
  unsigned TempCounter = 0;
  bool NumericOutputNeedBar = false;

  llvm::DenseMap<const FunctionDecl *, std::vector<const TagDecl *>> FunctionLocalTagsCache;

  mutable std::string AnonymousNamespaceName;

  static std::string getFirstExternalLinkageDeclName(ASTContext &Context) {
    for (const Decl *D : Context.getTranslationUnitDecl()->decls()) {
      if (D->isImplicit())
        continue;
      
      if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        if (FD->isThisDeclarationADefinition() && FD->hasExternalFormalLinkage()) {
          IdentifierInfo *II = FD->getIdentifier();
          if (II && !II->getName().empty()) {
            return II->getName().str();
          }
        }
      }
      
      if (const auto *VD = dyn_cast<VarDecl>(D)) {
        if (VD->isThisDeclarationADefinition() != VarDecl::DeclarationOnly &&
            VD->hasExternalFormalLinkage()) {
          IdentifierInfo *II = VD->getIdentifier();
          if (II && !II->getName().empty()) {
            return II->getName().str();
          }
        }
      }
    }
    return "";
  }

  void initAnonymousNamespaceName(ASTContext &Context) {
    std::string DeclName = getFirstExternalLinkageDeclName(Context);
    if (!DeclName.empty()) {
      AnonymousNamespaceName = "_GLOBAL_.N." + DeclName;
      return;
    }

    SourceManager &SM = Context.getSourceManager();
    FileID MainFileID = SM.getMainFileID();
    OptionalFileEntryRef MainFile = SM.getFileEntryRefForID(MainFileID);
    std::string FileName = MainFile ? MainFile->getName().str() : "stdin";

    std::string FormattedPath = FileName;
    for (char &C : FormattedPath) {
      if (!isalnum(C) && C != '.' && C != '$') {
        C = '_';
      }
    }

    uint64_t Hash = llvm::xxh3_64bits(FileName);
    std::string HashStr;
    const char CharSet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int I = 0; I < 6; ++I) {
      HashStr += CharSet[Hash % 62];
      Hash = Hash / 62;
    }

    AnonymousNamespaceName = "_GLOBAL_.N." + FormattedPath + HashStr;
  }

  unsigned getLocalTagUniquifier(const TagDecl *TD) {
    const DeclContext *DC = TD->getParentFunctionOrMethod();
    if (!DC)
      return 0;
    const auto *FD = dyn_cast<FunctionDecl>(DC);
    if (!FD)
      return 0;

    auto It = FunctionLocalTagsCache.find(FD);
    if (It == FunctionLocalTagsCache.end()) {
      std::vector<const TagDecl *> Tags;
      if (Stmt *Body = FD->getBody()) {
        LocalTagVisitor Visitor(FD, Tags);
        Visitor.TraverseStmt(Body);
      }
      It = FunctionLocalTagsCache.insert({FD, std::move(Tags)}).first;
    }

    const std::vector<const TagDecl *> &Tags = It->second;
    auto TagIt = llvm::find(Tags, TD);
    if (TagIt != Tags.end()) {
      return std::distance(Tags.begin(), TagIt);
    }
    return 0;
  }

public:
  explicit GCC2MangleContextImpl(ASTContext &Context, DiagnosticsEngine &Diags,
                                 bool IsAux = false)
      : GCC2MangleContext(Context, Diags, IsAux),
        Fallback(ItaniumMangleContext::create(Context, Diags, IsAux)) {}

  explicit GCC2MangleContextImpl(ASTContext &Context, DiagnosticsEngine &Diags,
                                 DiscriminatorOverrideTy Discriminator,
                                 bool IsAux = false)
      : GCC2MangleContext(Context, Diags, IsAux),
        Fallback(ItaniumMangleContext::create(Context, Diags, Discriminator,
                                              IsAux)) {}

  bool shouldMangleCXXName(const NamedDecl *D) override {
    return Fallback->shouldMangleCXXName(D);
  }

  bool shouldMangleStringLiteral(const StringLiteral *SL) override {
    return false;
  }

  bool isUniqueInternalLinkageDecl(const NamedDecl *ND) override {
    return Fallback->isUniqueInternalLinkageDecl(ND);
  }

  void needsUniqueInternalLinkageNames() override {
    Fallback->needsUniqueInternalLinkageNames();
  }

  void mangleCXXName(GlobalDecl GD, raw_ostream &Out) override;
  void mangleThunk(const CXXMethodDecl *MD, const ThunkInfo &Thunk,
                   bool ElideOverrideInfo, raw_ostream &Out) override;
  void mangleCXXDtorThunk(const CXXDestructorDecl *DD, CXXDtorType Type,
                          const ThunkInfo &Thunk, bool ElideOverrideInfo,
                          raw_ostream &Out) override;
  void mangleReferenceTemporary(const VarDecl *D, unsigned ManglingNumber,
                                raw_ostream &Out) override {
    Fallback->mangleReferenceTemporary(D, ManglingNumber, Out);
  }
  std::string getLambdaString(const CXXRecordDecl *Lambda) override {
    return Fallback->getLambdaString(Lambda);
  }
  void mangleCXXVTable(const CXXRecordDecl *RD, raw_ostream &Out) override;
  void mangleCXXVTT(const CXXRecordDecl *RD, raw_ostream &Out) override {
    Fallback->mangleCXXVTT(RD, Out);
  }
  void mangleCXXCtorVTable(const CXXRecordDecl *RD, int64_t Offset,
                           const CXXRecordDecl *Type, raw_ostream &Out) override {
    Fallback->mangleCXXCtorVTable(RD, Offset, Type, Out);
  }
  void mangleCXXRTTI(QualType T, raw_ostream &Out) override;
  void mangleCXXRTTIName(QualType T, raw_ostream &Out,
                         bool NormalizeIntegers) override;
  void mangleCanonicalTypeName(QualType T, raw_ostream &Out,
                               bool NormalizeIntegers) override {
    mangleType(T, Out);
  }

  void mangleCXXCtorComdat(const CXXConstructorDecl *D, raw_ostream &Out) override {
    Fallback->mangleCXXCtorComdat(D, Out);
  }
  void mangleCXXDtorComdat(const CXXDestructorDecl *D, raw_ostream &Out) override {
    Fallback->mangleCXXDtorComdat(D, Out);
  }
  void mangleStaticGuardVariable(const VarDecl *D, raw_ostream &Out) override;
  void mangleDynamicInitializer(const VarDecl *D, raw_ostream &Out) override {
    Fallback->mangleDynamicInitializer(D, Out);
  }
  void mangleDynamicAtExitDestructor(const VarDecl *D, raw_ostream &Out) override {
    Out << "__dtor_";
    if (D->getDeclContext()->isTranslationUnit()) {
      Out << D->getName();
    } else {
      mangleCXXName(D, Out);
    }
  }
  void mangleDynamicStermFinalizer(const VarDecl *D, raw_ostream &Out) override {
    Fallback->mangleDynamicStermFinalizer(D, Out);
  }
  void mangleSEHFilterExpression(GlobalDecl EnclosingDecl,
                                 raw_ostream &Out) override {
    Fallback->mangleSEHFilterExpression(EnclosingDecl, Out);
  }
  void mangleSEHFinallyBlock(GlobalDecl EnclosingDecl,
                             raw_ostream &Out) override {
    Fallback->mangleSEHFinallyBlock(EnclosingDecl, Out);
  }
  void mangleItaniumThreadLocalInit(const VarDecl *D, raw_ostream &Out) override {
    Fallback->mangleItaniumThreadLocalInit(D, Out);
  }
  void mangleItaniumThreadLocalWrapper(const VarDecl *D, raw_ostream &Out) override {
    Fallback->mangleItaniumThreadLocalWrapper(D, Out);
  }

  void mangleStringLiteral(const StringLiteral *SL, raw_ostream &Out) override {
    Fallback->mangleStringLiteral(SL, Out);
  }
  void mangleLambdaSig(const CXXRecordDecl *Lambda, raw_ostream &Out) override {
    Fallback->mangleLambdaSig(Lambda, Out);
  }

  void mangleModuleInitializer(const Module *Module, raw_ostream &Out) override {
    Fallback->mangleModuleInitializer(Module, Out);
  }

  DiscriminatorOverrideTy getDiscriminatorOverride() const override {
    return Fallback->getDiscriminatorOverride();
  }

private:
  bool isBackReferenceableType(QualType T);
  void mangleParameterList(ArrayRef<const ParmVarDecl *> Parameters, raw_ostream &Out);
  void mangleType(QualType T, raw_ostream &Out, bool IsTopLevelParm = false);
  void manglePrefix(const DeclContext *DC, raw_ostream &Out);
  void mangleTemplateParameterList(const TemplateParameterList *Params, raw_ostream &Out);
  void mangleTemplateArgs(const TemplateParameterList *Params,
                          const TemplateArgumentList &TemplateArgs,
                          raw_ostream &Out);
};

void GCC2MangleContextImpl::mangleTemplateParameterList(const TemplateParameterList *Params, raw_ostream &Out) {
  Out << Params->size();
  for (const NamedDecl *Param : *Params) {
    if (isa<TemplateTypeParmDecl>(Param)) {
      Out << "Z";
    } else if (const auto *NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
      mangleType(NTTP->getType(), Out);
    } else if (const auto *TTP = dyn_cast<TemplateTemplateParmDecl>(Param)) {
      Out << "z";
      mangleTemplateParameterList(TTP->getTemplateParameters(), Out);
    }
  }
}

void GCC2MangleContextImpl::mangleTemplateArgs(const TemplateParameterList *Params,
                                               const TemplateArgumentList &TemplateArgs,
                                               raw_ostream &Out) {
  Out << TemplateArgs.size();
  for (unsigned ArgI = 0; ArgI < TemplateArgs.size(); ++ArgI) {
    const TemplateArgument &Arg = TemplateArgs[ArgI];
    NumericOutputNeedBar = false;
    if (Arg.getKind() == TemplateArgument::Type) {
      Out << "Z";
      mangleType(Arg.getAsType(), Out);
    } else if (Arg.getKind() == TemplateArgument::Integral) {
      mangleType(Arg.getIntegralType(), Out);
      llvm::APSInt Val = Arg.getAsIntegral();
      if (Val.isNegative()) {
        Out << "m";
        Val = -Val;
      }
      Out << Val;
      NumericOutputNeedBar = true;
    } else if (Arg.getKind() == TemplateArgument::StructuralValue) {
      QualType T = Arg.getStructuralValueType();
      if (T->isRealFloatingType()) {
        mangleType(T, Out);
        const APValue &Val = Arg.getAsStructuralValue();
        assert(Val.isFloat() && "expected float value for real floating type");
        llvm::APFloat FloatVal = Val.getFloat();
        if (FloatVal.isNaN()) {
          Out << "NaN";
        } else if (FloatVal.isInfinity()) {
          if (FloatVal.isNegative()) {
            Out << "m";
          }
          Out << "Infinity";
        } else {
          double DVal = FloatVal.convertToDouble();
          char Buffer[128];
          if (FloatVal.isNegative()) {
            Out << "m";
            DVal = -DVal;
          }
          snprintf(Buffer, sizeof(Buffer), "%.20e", DVal);
          char *Exponent = strchr(Buffer, 'e');
          if (!Exponent) {
            Out << Buffer << "e0";
          } else {
            *Exponent = '\0';
            Out << Buffer << "e";
            char *ExpPtr = Exponent + 1;
            if (*ExpPtr == '-') {
              Out << "m";
              ExpPtr++;
            } else if (*ExpPtr == '+') {
              ExpPtr++;
            }
            while (*ExpPtr == '0') {
              ExpPtr++;
            }
            if (*ExpPtr == '\0') {
              Out << "0";
            } else {
              Out << ExpPtr;
            }
          }
        }
      }
    } else if (Arg.getKind() == TemplateArgument::Template) {
      Out << "z";
      TemplateName TName = Arg.getAsTemplate();
      if (TemplateDecl *TD = TName.getAsTemplateDecl()) {
        mangleTemplateParameterList(TD->getTemplateParameters(), Out);
        StringRef TNameStr = TD->getName();
        Out << TNameStr.size() << TNameStr;
      }
    } else if (Arg.getKind() == TemplateArgument::Declaration) {
      if (Params && ArgI < Params->size()) {
        if (const auto *NTTP = dyn_cast<NonTypeTemplateParmDecl>(Params->getParam(ArgI))) {
          std::string TypeMangled;
          llvm::raw_string_ostream TypeOut(TypeMangled);
          mangleType(NTTP->getType(), TypeOut);
          Out << TypeMangled;
          if (!TypeMangled.empty() && TypeMangled.back() >= '0' && TypeMangled.back() <= '9') {
            NumericOutputNeedBar = true;
          }
        }
      }
      ValueDecl *D = Arg.getAsDecl();
      if (const auto *MD = dyn_cast<CXXMethodDecl>(D)) {
        const CXXRecordDecl *RD = MD->getParent();
        const CXXRecordDecl *Base = RD;
        const CXXMethodDecl *OrigMD = MD;
        int64_t Delta = 0;

        if (MD->size_overridden_methods() > 0) {
          const CXXMethodDecl *TempMD = MD;
          while (TempMD->size_overridden_methods() > 0) {
            TempMD = *TempMD->overridden_methods().begin();
          }
          const CXXRecordDecl *TempBase = TempMD->getParent();
          OrigMD = TempMD;
          if (RD->isVirtuallyDerivedFrom(TempBase)) {
            // GCC 2.95 virtual base PMF bug: keep derived class context!
            Base = RD;
          } else {
            Base = TempBase;
            Delta = getBaseOffset(getASTContext(), RD, Base).getQuantity();
          }
        }

        Out << Delta << "_";
        if (MD->isVirtual()) {
          auto *VTC = cast<GCC2VTableContext>(getASTContext().getVTableContext());
          uint64_t VTableIndex = VTC->getMethodVTableIndex(OrigMD);
          const ASTRecordLayout &BaseLayout = getASTContext().getASTRecordLayout(Base);
          int64_t VFPtrOffset = BaseLayout.getVFPtrOffset().getQuantity();
          if (VFPtrOffset > 0 && !BaseLayout.hasOwnVFPtr() && !BaseLayout.getPrimaryBase()) {
            VFPtrOffset = 0;
          }
          int64_t Delta2 = Delta + VFPtrOffset;
          Out << (VTableIndex + 1) << "_i" << Delta2;
        } else {
          Out << "m1_";
          if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
          std::string Mangled;
          llvm::raw_string_ostream MangledOut(Mangled);
          mangleCXXName(OrigMD, MangledOut);
          Out << Mangled.size() << Mangled;
          if (!Mangled.empty() && Mangled.back() >= '0' && Mangled.back() <= '9')
            NumericOutputNeedBar = true;
          else
            NumericOutputNeedBar = false;
        }
      } else if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
        std::string Mangled;
        llvm::raw_string_ostream MangledOut(Mangled);
        mangleCXXName(FD, MangledOut);
        Out << Mangled.size() << Mangled;
        if (!Mangled.empty() && Mangled.back() >= '0' && Mangled.back() <= '9')
          NumericOutputNeedBar = true;
        else
          NumericOutputNeedBar = false;
      } else if (const auto *VD = dyn_cast<VarDecl>(D)) {
        if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
        std::string Mangled;
        llvm::raw_string_ostream MangledOut(Mangled);
        mangleCXXName(VD, MangledOut);
        Out << Mangled.size() << Mangled;
        if (!Mangled.empty() && Mangled.back() >= '0' && Mangled.back() <= '9')
          NumericOutputNeedBar = true;
        else
          NumericOutputNeedBar = false;
      } else if (const auto *FD = dyn_cast<FieldDecl>(D)) {
        StringRef Name = FD->getName();
        if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
        Out << Name.size() << Name;
        if (!Name.empty() && Name.back() >= '0' && Name.back() <= '9')
          NumericOutputNeedBar = true;
        else
          NumericOutputNeedBar = false;
      }
    }
  }
}

void GCC2MangleContextImpl::manglePrefix(const DeclContext *DC, raw_ostream &Out) {
  if (DC->isTranslationUnit())
    return;

  SmallVector<const DeclContext *, 8> Contexts;
  for (const DeclContext *C = DC; !C->isTranslationUnit(); C = C->getParent()) {
    if (isIgnoredStdNamespace(C))
      continue;
    if (isa<LinkageSpecDecl>(C))
      continue;
    Contexts.push_back(C);
  }

  if (Contexts.size() > 1) {
    NumericOutputNeedBar = false;
    if (Contexts.size() > 9)
      Out << "Q_" << Contexts.size() << "_";
    else
      Out << "Q" << Contexts.size();
  }

  for (auto I = Contexts.rbegin(), E = Contexts.rend(); I != E; ++I) {
    if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(*I)) {
      Out << "t";
      StringRef Name = Spec->getName();
      if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
      Out << Name.size() << Name;
      const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
      const TemplateParameterList *Params = Spec->getSpecializedTemplate()->getTemplateParameters();
      mangleTemplateArgs(Params, TemplateArgs, Out);
      continue;
    }
    if (const auto *RD = dyn_cast<CXXRecordDecl>(*I)) {
      if (const auto *FD = dyn_cast_or_null<FunctionDecl>(RD->getDeclContext())) {
        if (FunctionTemplateDecl *PrimaryTemplate = FD->getPrimaryTemplate()) {
          if (FD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization) {
            // A local class inside a template function is mangled as a template class
            // using the template arguments of its enclosing function.
            Out << "t";
            StringRef Name = RD->getName();
            if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
            Out << Name.size() << Name;
            const TemplateArgumentList *Args = FD->getTemplateSpecializationArgs();
            const TemplateParameterList *Params = PrimaryTemplate->getTemplateParameters();
            mangleTemplateArgs(Params, *Args, Out);
            continue;
          }
        }
      }
    }
    if (const auto *FD = dyn_cast<FunctionDecl>(*I)) {
      // Enclosing function context: recursively mangle and append uniquifier
      std::string Mangled;
      llvm::raw_string_ostream MangledOut(Mangled);
      mangleCXXName(GlobalDecl(FD), MangledOut);
      
      unsigned UniquifierVal = 0;
      if (!Contexts.empty()) {
        if (const auto *TD = dyn_cast<TagDecl>(Contexts[0])) {
          UniquifierVal = getLocalTagUniquifier(TD);
        }
      }
      MangledOut << "." << UniquifierVal;
      MangledOut.flush();

      if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
      Out << Mangled.size() << Mangled;
      if (I + 1 != E) {
        const DeclContext *NextC = *(I + 1);
        bool NextStartsWithT = false;
        if (const auto *NextRD = dyn_cast<CXXRecordDecl>(NextC)) {
          if (isa<ClassTemplateSpecializationDecl>(NextRD)) {
            NextStartsWithT = true;
          } else if (const auto *NextFD = dyn_cast_or_null<FunctionDecl>(NextRD->getDeclContext())) {
            if (NextFD->getPrimaryTemplate()) {
              if (NextFD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization) {
                NextStartsWithT = true;
              }
            }
          }
        }
        if (!NextStartsWithT) {
          Out << "_";
        }
      }
      continue;
    }
    if (const auto *NS = dyn_cast<NamespaceDecl>(*I)) {
      if (NS->isAnonymousNamespace()) {
        if (NumericOutputNeedBar) { Out << "_"; NumericOutputNeedBar = false; }
        if (AnonymousNamespaceName.empty()) {
          initAnonymousNamespaceName(getASTContext());
        }
        Out << AnonymousNamespaceName.size() << AnonymousNamespaceName;
        continue;
      }
    }
    if (const auto *ND = dyn_cast<NamedDecl>(*I)) {
      StringRef Name = ND->getName();
      if (NumericOutputNeedBar && isa<NamespaceDecl>(ND)) { Out << "_"; NumericOutputNeedBar = false; }
      Out << Name.size() << Name;
    }
  }
}


bool GCC2MangleContextImpl::isBackReferenceableType(QualType T) {
  T = T.getCanonicalType();
  if (T->isBooleanType())
    return true;
  if (T->isBuiltinType())
    return false;
  if (isa<TemplateTypeParmType>(T.getTypePtr()))
    return false;
  return true;
}

static QualType getGCC2CanonicalType(ASTContext &Context, QualType T) {
  Qualifiers Quals = T.getQualifiers();
  QualType UnqualT = T.getCanonicalType().getUnqualifiedType();

  if (const auto *RT = UnqualT->getAs<ReferenceType>()) {
    QualType Pointee = getGCC2CanonicalType(Context, RT->getPointeeType());
    QualType Result;
    if (isa<LValueReferenceType>(RT))
      Result = Context.getLValueReferenceType(Pointee);
    else
      Result = Context.getRValueReferenceType(Pointee);
    return Context.getQualifiedType(Result, Quals);
  }

  if (const auto *PT = UnqualT->getAs<PointerType>()) {
    QualType Pointee = getGCC2CanonicalType(Context, PT->getPointeeType());
    QualType Result = Context.getPointerType(Pointee);
    return Context.getQualifiedType(Result, Quals);
  }

  if (const auto *MPT = UnqualT->getAs<MemberPointerType>()) {
    if (MPT->isMemberFunctionPointerType()) {
      QualType Pointee = MPT->getPointeeType().getUnqualifiedType();
      Pointee = getGCC2CanonicalType(Context, Pointee);
      return Context.getMemberPointerType(Pointee, MPT->getQualifier(), MPT->getMostRecentCXXRecordDecl());
    } else {
      QualType Pointee = getGCC2CanonicalType(Context, MPT->getPointeeType());
      QualType Result = Context.getMemberPointerType(Pointee, MPT->getQualifier(), MPT->getMostRecentCXXRecordDecl());
      return Context.getQualifiedType(Result, Quals);
    }
  }

  return T.getCanonicalType();
}

void GCC2MangleContextImpl::mangleParameterList(ArrayRef<const ParmVarDecl *> Parameters,
                                                raw_ostream &Out) {
  SmallVector<QualType, 8> TypeVec;

  auto pushType = [&](QualType T) {
    TypeVec.push_back(getGCC2CanonicalType(getASTContext(), T));
  };

  bool NeedPadding = false;
  const CXXRecordDecl *MethodParent = nullptr;
  if (!Parameters.empty()) {
    const DeclContext *DC = Parameters[0]->getDeclContext();
    if (const auto *FD = dyn_cast<FunctionDecl>(DC)) {
      if (isa<CXXMethodDecl>(FD)) { // All member functions
        NeedPadding = true;
        MethodParent = cast<CXXMethodDecl>(FD)->getParent();
      } else { // Global function
        const DeclContext *FuncDC = FD->getDeclContext();
        while (FuncDC && isa<LinkageSpecDecl>(FuncDC))
          FuncDC = FuncDC->getParent();
        if (FuncDC && FuncDC->isNamespace() && !FuncDC->isTranslationUnit()) {
          if (!isIgnoredStdNamespace(FuncDC)) {
            NeedPadding = true;
          }
        }
      }
    }
  }

  if (NeedPadding) {
    if (MethodParent) {
      QualType ClassTy = getASTContext().getCanonicalTagType(MethodParent);
      pushType(ClassTy.getCanonicalType());
    } else {
      pushType(getASTContext().VoidTy);
    }
  }

  unsigned NRepeats = 0;
  QualType LastType;

  auto flushRepeats = [&](unsigned Repeats, QualType T) -> bool {
    if (!isBackReferenceableType(T)) {
      return false;
    }
    if (TypeVec.empty()) {
      return false;
    }

    QualType CanonicalT = getGCC2CanonicalType(getASTContext(), T);
    auto It = llvm::find(ArrayRef<QualType>(TypeVec.begin(), TypeVec.end() - 1), CanonicalT);
    if (It == TypeVec.end() - 1) {
      return false;
    }
    unsigned Index = std::distance(ArrayRef<QualType>(TypeVec).begin(), It);
    if (Repeats > 1) {
      Out << "N" << Repeats;
      if (Repeats > 9) Out << "_";
    } else {
      Out << "T";
    }
    Out << Index;
    if (Index > 9) Out << "_";
    return true;
  };

  for (const ParmVarDecl *PD : Parameters) {
    QualType ParmType = PD->getASTContext().getSignatureParameterType(PD->getType()).getCanonicalType();
    pushType(ParmType);

    if (!LastType.isNull() && ParmType == LastType) {
      if (isBackReferenceableType(ParmType)) {
        NRepeats++;
        continue;
      }
    } else if (NRepeats != 0) {
      flushRepeats(NRepeats, LastType);
      NRepeats = 0;
    }

    LastType = ParmType;

    if (flushRepeats(0, ParmType))
      continue;

    mangleType(ParmType, Out, /*IsTopLevelParm=*/true);
  }

  if (NRepeats != 0) {
    flushRepeats(NRepeats, LastType);
    NRepeats = 0;
  }
}

void GCC2MangleContextImpl::mangleType(QualType T, raw_ostream &Out, bool IsTopLevelParm) {
  T = T.getCanonicalType();

  QualType UnqualT = T.getLocalUnqualifiedType();

  bool IsUnsigned = false;
  if (const auto *BT = UnqualT->getAs<BuiltinType>()) {
    switch (BT->getKind()) {
    case BuiltinType::UChar:
    case BuiltinType::UShort:
    case BuiltinType::UInt:
    case BuiltinType::ULong:
    case BuiltinType::ULongLong:
      IsUnsigned = true;
      break;
    default:
      break;
    }
  }

  if (T.isConstQualified() && !T->isMemberFunctionPointerType() && !T->isArrayType())
    Out << "C";
  if (IsUnsigned)
    Out << "U";
  if (T.isVolatileQualified() && !T->isMemberFunctionPointerType() && !T->isArrayType())
    Out << "V";
  if (T.isRestrictQualified() && !T->isArrayType())
    Out << "u";

  if (const auto *RT = UnqualT->getAs<ReferenceType>()) {
    Out << "R";
    mangleType(RT->getPointeeType(), Out);
    return;
  }
  if (const auto *PT = UnqualT->getAs<PointerType>()) {
    Out << "P";
    mangleType(PT->getPointeeType(), Out);
    return;
  }
  if (const auto *MPT = UnqualT->getAs<MemberPointerType>()) {
    Out << "P";
    const CXXRecordDecl *RD = MPT->getMostRecentCXXRecordDecl();
    QualType Pointee = MPT->getPointeeType();
    if (MPT->isMemberDataPointer()) {
      Out << "O";
      manglePrefix(RD, Out);
      Out << "_";
      mangleType(Pointee, Out);
    } else {
      Out << "M";
      manglePrefix(RD, Out);
      const auto *FPT = Pointee->castAs<FunctionProtoType>();
      if (FPT->getMethodQuals().hasConst())
        Out << "C";
      if (FPT->getMethodQuals().hasVolatile())
        Out << "V";
      Out << "F";
      Out << "P";
      if (FPT->getMethodQuals().hasConst())
        Out << "C";
      if (FPT->getMethodQuals().hasVolatile())
        Out << "V";
      manglePrefix(RD, Out);
      for (const auto &ParamTy : FPT->param_types())
        mangleType(ParamTy, Out);
      Out << "_";
      mangleType(FPT->getReturnType(), Out);
    }
    return;
  }

  const Type *Ty = UnqualT.getTypePtr();
  switch (Ty->getTypeClass()) {
  case Type::Builtin: {
    switch (cast<BuiltinType>(Ty)->getKind()) {
    case BuiltinType::Void: Out << "v"; break;
    case BuiltinType::Bool: Out << "b"; break;
    case BuiltinType::Char_U:
    case BuiltinType::Char_S: Out << "c"; break;
    case BuiltinType::UChar: Out << "c"; break;
    case BuiltinType::SChar: Out << "Sc"; break;
    case BuiltinType::WChar_U:
    case BuiltinType::WChar_S: Out << "w"; break;
    case BuiltinType::Short: Out << "s"; break;
    case BuiltinType::UShort: Out << "s"; break;
    case BuiltinType::Int: Out << "i"; break;
    case BuiltinType::UInt: Out << "i"; break;
    case BuiltinType::Long: Out << "l"; break;
    case BuiltinType::ULong: Out << "l"; break;
    case BuiltinType::LongLong: Out << "x"; break;
    case BuiltinType::ULongLong: Out << "x"; break;
    case BuiltinType::Float: Out << "f"; break;
    case BuiltinType::Double: Out << "d"; break;
    case BuiltinType::LongDouble: Out << "r"; break;
    default: Fallback->mangleCanonicalTypeName(UnqualT, Out); break;
    }
    break;
  }
  case Type::Record:
  case Type::Enum: {
    const TagDecl *TD = cast<TagType>(Ty)->getDecl();
    if (IsTopLevelParm && Ty->isRecordType())
      Out << "G";
    manglePrefix(TD, Out);
    break;
  }
  case Type::Complex: {
    const auto *CT = cast<ComplexType>(Ty);
    Out << "J";
    mangleType(CT->getElementType(), Out);
    break;
  }
  case Type::ConstantArray: {
    const auto *CAT = cast<ConstantArrayType>(Ty);
    int64_t Size = CAT->getSize().getZExtValue();
    int64_t Bounds = Size - 1;
    if (Bounds < 0) {
      Out << "Am" << -Bounds << "_";
    } else {
      Out << "A" << Bounds << "_";
    }
    Qualifiers Quals = T.getQualifiers();
    QualType ElemTy = CAT->getElementType();
    Quals.addQualifiers(ElemTy.getQualifiers());
    ElemTy = getASTContext().getQualifiedType(ElemTy.getUnqualifiedType(), Quals);
    mangleType(ElemTy, Out);
    break;
  }
  case Type::IncompleteArray:
  case Type::VariableArray:
  case Type::DependentSizedArray: {
    const auto *AT = cast<ArrayType>(Ty);
    Out << "P";
    Qualifiers Quals = T.getQualifiers();
    QualType ElemTy = AT->getElementType();
    Quals.addQualifiers(ElemTy.getQualifiers());
    ElemTy = getASTContext().getQualifiedType(ElemTy.getUnqualifiedType(), Quals);
    mangleType(ElemTy, Out);
    break;
  }
  case Type::FunctionProto: {
    const auto *FPT = cast<FunctionProtoType>(Ty);
    Out << "F";
    if (FPT->param_types().empty())
      Out << "v";
    else
      for (const auto &ParamTy : FPT->param_types())
        mangleType(ParamTy, Out);
    Out << "_";
    mangleType(FPT->getReturnType(), Out);
    break;
  }
  case Type::FunctionNoProto: {
    const auto *FPT = cast<FunctionNoProtoType>(Ty);
    Out << "Fv_";
    mangleType(FPT->getReturnType(), Out);
    break;
  }
  case Type::TemplateTypeParm: {
    const auto *TTP = cast<TemplateTypeParmType>(Ty);
    Out << "X";
    unsigned Index = TTP->getIndex();
    unsigned Depth = TTP->getDepth();
    unsigned Level = Depth + 1;
    auto printUnderscoreInt = [&](unsigned Val) {
      if (Val > 9) Out << "_";
      Out << Val;
      if (Val > 9) Out << "_";
    };
    printUnderscoreInt(Index);
    printUnderscoreInt(Level);
    break;
  }
  default:
    Fallback->mangleCanonicalTypeName(UnqualT, Out);
    break;
  }
}

void GCC2MangleContextImpl::mangleCXXName(GlobalDecl GD, raw_ostream &Out) {
  NumericOutputNeedBar = false;
  const NamedDecl *D = cast<NamedDecl>(GD.getDecl());

  if (const auto *CD = dyn_cast<CXXConstructorDecl>(D)) {
    Out << "__";
    manglePrefix(CD->getParent(), Out);
    if (CD->getParent()->getNumVBases() != 0)
      mangleType(CD->getASTContext().IntTy, Out);
    mangleParameterList(CD->parameters(), Out);
    return;
  }

  if (const auto *DD = dyn_cast<CXXDestructorDecl>(D)) {
    Out << "_._";
    manglePrefix(DD->getParent(), Out);
    return;
  }

  if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
    if (FD->getDeclName().getNameKind() == DeclarationName::CXXOperatorName) {
      switch (FD->getDeclName().getCXXOverloadedOperator()) {
      case OO_EqualEqual: Out << "__eq"; break;
      case OO_ExclaimEqual: Out << "__ne"; break;
      case OO_Less: Out << "__lt"; break;
      case OO_Greater: Out << "__gt"; break;
      case OO_LessEqual: Out << "__le"; break;
      case OO_GreaterEqual: Out << "__ge"; break;
      case OO_Spaceship: Out << "__cmp"; break;
      case OO_Equal: Out << "__as"; break;
      case OO_Plus: Out << "__pl"; break;
      case OO_Minus: Out << "__mi"; break;
      case OO_Star: Out << "__ml"; break;
      case OO_Slash: Out << "__dv"; break;
      case OO_Percent: Out << "__md"; break;
      case OO_Amp: Out << "__ad"; break;
      case OO_Pipe: Out << "__or"; break;
      case OO_Caret: Out << "__er"; break;
      case OO_Tilde: Out << "__co"; break;
      case OO_Exclaim: Out << "__nt"; break;
      case OO_Call: Out << "__cl"; break;
      case OO_Subscript: Out << "__vc"; break;
      case OO_Arrow: Out << "__rf"; break;
      case OO_ArrowStar: Out << "__rm"; break;
      case OO_LessLess: Out << "__ls"; break;
      case OO_GreaterGreater: Out << "__rs"; break;
      case OO_AmpAmp: Out << "__aa"; break;
      case OO_PipePipe: Out << "__oo"; break;
      case OO_PlusPlus: Out << "__pp"; break;
      case OO_MinusMinus: Out << "__mm"; break;
      case OO_PlusEqual: Out << "__apl"; break;
      case OO_MinusEqual: Out << "__ami"; break;
      case OO_StarEqual: Out << "__aml"; break;
      case OO_SlashEqual: Out << "__adv"; break;
      case OO_PercentEqual: Out << "__amd"; break;
      case OO_AmpEqual: Out << "__aad"; break;
      case OO_PipeEqual: Out << "__aor"; break;
      case OO_CaretEqual: Out << "__aer"; break;
      case OO_LessLessEqual: Out << "__als"; break;
      case OO_GreaterGreaterEqual: Out << "__ars"; break;
      case OO_Comma: Out << "__cm"; break;
      case OO_New:
        if (!isa<CXXMethodDecl>(FD) && FD->getNumParams() == 1) {
          Out << "__builtin_new";
          return;
        }
        Out << "__nw";
        break;
      case OO_Delete:
        if (!isa<CXXMethodDecl>(FD) && FD->getNumParams() == 1) {
          Out << "__builtin_delete";
          return;
        }
        Out << "__dl";
        break;
      case OO_Array_New:
        if (!isa<CXXMethodDecl>(FD) && FD->getNumParams() == 1) {
          Out << "__builtin_vec_new";
          return;
        }
        Out << "__vn";
        break;
      case OO_Array_Delete:
        if (!isa<CXXMethodDecl>(FD) && FD->getNumParams() == 1) {
          Out << "__builtin_vec_delete";
          return;
        }
        Out << "__vd";
        break;
      default: Fallback->mangleCXXName(GD, Out); return;
      }
    } else if (FD->getDeclName().getNameKind() == DeclarationName::CXXConversionFunctionName) {
      Out << "__op";
      mangleType(FD->getReturnType(), Out);
    } else {
      Out << FD->getName();
    }
    if (FunctionTemplateDecl *PrimaryTemplate = FD->getPrimaryTemplate()) {
      const TemplateParameterList *Params = PrimaryTemplate->getTemplateParameters();
      const TemplateArgumentList *Args = FD->getTemplateSpecializationArgs();
      const FunctionDecl *TemplatedFD = PrimaryTemplate->getTemplatedDecl();
      const CXXMethodDecl *TemplatedMD = dyn_cast<CXXMethodDecl>(TemplatedFD);
      const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(FD);
      if (TemplatedMD) {
        Out << "__H";
        mangleTemplateArgs(Params, *Args, Out);
        Out << "_";
        if (MD->isConst()) Out << "C";
        if (MD->isVolatile()) Out << "V";
        manglePrefix(MD->getParent(), Out);
        mangleParameterList(TemplatedMD->parameters(), Out);
      } else {
        Out << "__H";
        mangleTemplateArgs(Params, *Args, Out);
        Out << "_";
        if (!isEffectivelyTranslationUnit(FD->getDeclContext())) {
          manglePrefix(FD->getDeclContext(), Out);
        }
        if (TemplatedFD->parameters().empty())
          Out << "v";
        else
          mangleParameterList(TemplatedFD->parameters(), Out);
      }
      if (!isa<CXXConstructorDecl>(FD)) {
        Out << "_";
        mangleType(TemplatedFD->getReturnType(), Out);
      }
    } else {
      if (const auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
          Out << "__";
          if (MD->isConst()) Out << "C";
          if (MD->isVolatile()) Out << "V";
          manglePrefix(MD->getParent(), Out);
          mangleParameterList(FD->parameters(), Out);
        } else {
          if (!isEffectivelyTranslationUnit(FD->getDeclContext())) {
            Out << "__";
            manglePrefix(FD->getDeclContext(), Out);
            if (FD->parameters().empty())
              Out << "v";
            else
              mangleParameterList(FD->parameters(), Out);
          } else {
            Out << "__F";
            if (FD->parameters().empty())
              Out << "v";
            else
              mangleParameterList(FD->parameters(), Out);
          }
        }
      }
    return;
  }

  if (const auto *VD = dyn_cast<VarDecl>(D)) {
    const DeclContext *DC = VD->getDeclContext();
    if ((DC->isRecord() || DC->isNamespace()) && !isEffectivelyTranslationUnit(DC)) {
      Out << "_";
      manglePrefix(DC, Out);
      Out << "." << VD->getName();
      return;
    }
  }

  Out << D->getName();
}

void GCC2MangleContextImpl::mangleThunk(const CXXMethodDecl *MD,
                                        const ThunkInfo &Thunk,
                                        bool ElideOverrideInfo,
                                        raw_ostream &Out) {
  Out << "__thunk_";
  if (Thunk.This.NonVirtual > 0)
    Out << "n" << Thunk.This.NonVirtual;
  else
    Out << -Thunk.This.NonVirtual;
  Out << "_";
  mangleCXXName(MD, Out);
}

void GCC2MangleContextImpl::mangleCXXDtorThunk(const CXXDestructorDecl *DD,
                                               CXXDtorType Type,
                                               const ThunkInfo &Thunk,
                                               bool ElideOverrideInfo,
                                               raw_ostream &Out) {
  Out << "__thunk_";
  if (Thunk.This.NonVirtual > 0)
    Out << "n" << Thunk.This.NonVirtual;
  else
    Out << -Thunk.This.NonVirtual;
  Out << "_";
  mangleCXXName(GlobalDecl(DD, Dtor_Complete), Out);
}

void GCC2MangleContextImpl::mangleCXXVTable(const CXXRecordDecl *RD,
                                            raw_ostream &Out) {
  NumericOutputNeedBar = false;
  Out << "__vt_";
  manglePrefix(RD, Out);
}

void GCC2MangleContextImpl::mangleCXXRTTI(QualType T, raw_ostream &Out) {
  NumericOutputNeedBar = false;
  Out << "__tf";
  mangleType(T, Out);
}

void GCC2MangleContextImpl::mangleCXXRTTIName(QualType T, raw_ostream &Out,
                                              bool NormalizeIntegers) {
  NumericOutputNeedBar = false;
  Out << "__ti";
  mangleType(T, Out);
}

void GCC2MangleContextImpl::mangleStaticGuardVariable(const VarDecl *D,
                                                      raw_ostream &Out) {
  Out << "__tmp_" << TempCounter++;
}

}

GCC2MangleContext *
GCC2MangleContext::create(ASTContext &Context, DiagnosticsEngine &Diags,
                          bool IsAux) {
  return new GCC2MangleContextImpl(Context, Diags, IsAux);
}

GCC2MangleContext *
GCC2MangleContext::create(ASTContext &Context, DiagnosticsEngine &Diags,
                          DiscriminatorOverrideTy Discriminator, bool IsAux) {
  return new GCC2MangleContextImpl(Context, Diags, Discriminator, IsAux);
}
