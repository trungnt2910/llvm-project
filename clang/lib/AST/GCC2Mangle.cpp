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
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace clang;

namespace {

class GCC2MangleContextImpl : public GCC2MangleContext {
  std::unique_ptr<ItaniumMangleContext> Fallback;
  llvm::DenseMap<const NamedDecl *, unsigned> Uniquifier;
  unsigned TempCounter = 0;

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
    Fallback->mangleDynamicAtExitDestructor(D, Out);
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
  void mangleType(QualType T, raw_ostream &Out);
  void manglePrefix(const DeclContext *DC, raw_ostream &Out);
};

void GCC2MangleContextImpl::manglePrefix(const DeclContext *DC, raw_ostream &Out) {
  if (DC->isTranslationUnit())
    return;

  SmallVector<const DeclContext *, 8> Contexts;
  for (const DeclContext *C = DC; !C->isTranslationUnit(); C = C->getParent()) {
    if (const auto *ND = dyn_cast<NamespaceDecl>(C))
      if (ND->isStdNamespace() || ND->getIdentifier()->isStr("std"))
        continue;
    if (isa<LinkageSpecDecl>(C))
      continue;
    Contexts.push_back(C);
  }

  if (Contexts.size() > 1)
    Out << "Q" << Contexts.size();

  for (auto I = Contexts.rbegin(), E = Contexts.rend(); I != E; ++I) {
    if (const auto *ND = dyn_cast<NamedDecl>(*I)) {
      StringRef Name = ND->getName();
      Out << Name.size() << Name;
    }
  }
}

void GCC2MangleContextImpl::mangleType(QualType T, raw_ostream &Out) {
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

  if (T.isConstQualified() && !T->isMemberFunctionPointerType())
    Out << "C";
  if (IsUnsigned)
    Out << "U";
  if (T.isVolatileQualified() && !T->isMemberFunctionPointerType())
    Out << "V";
  if (T.isRestrictQualified())
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
    Out << "A" << (CAT->getSize().getZExtValue() - 1) << "_";
    mangleType(CAT->getElementType(), Out);
    break;
  }
  case Type::IncompleteArray:
  case Type::VariableArray:
  case Type::DependentSizedArray: {
    const auto *AT = cast<ArrayType>(Ty);
    Out << "P";
    mangleType(AT->getElementType(), Out);
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
  default:
    Fallback->mangleCanonicalTypeName(UnqualT, Out);
    break;
  }
}

void GCC2MangleContextImpl::mangleCXXName(GlobalDecl GD, raw_ostream &Out) {
  const NamedDecl *D = cast<NamedDecl>(GD.getDecl());

  if (const auto *CD = dyn_cast<CXXConstructorDecl>(D)) {
    Out << "__";
    manglePrefix(CD->getParent(), Out);
    if (CD->getParent()->getNumVBases() != 0)
      mangleType(CD->getASTContext().IntTy, Out);
    for (const auto *PD : CD->parameters())
      mangleType(PD->getASTContext().getSignatureParameterType(PD->getType()), Out);
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
    } else {
      Out << FD->getName();
    }
    if (const auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
      Out << "__";
      if (MD->isConst()) Out << "C";
      if (MD->isVolatile()) Out << "V";
      manglePrefix(MD->getParent(), Out);
      for (const auto *PD : FD->parameters())
        mangleType(PD->getASTContext().getSignatureParameterType(PD->getType()), Out);
    } else {
      Out << "__F";
      if (FD->parameters().empty())
        Out << "v";
      else
        for (const auto *PD : FD->parameters())
          mangleType(PD->getASTContext().getSignatureParameterType(PD->getType()), Out);
    }
    return;
  }

  if (const auto *VD = dyn_cast<VarDecl>(D)) {
    if (VD->isStaticDataMember()) {
      Out << "_";
      manglePrefix(VD->getDeclContext(), Out);
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
  Out << "__vt_";
  manglePrefix(RD, Out);
}

void GCC2MangleContextImpl::mangleCXXRTTI(QualType T, raw_ostream &Out) {
  Out << "__tf";
  mangleType(T, Out);
}

void GCC2MangleContextImpl::mangleCXXRTTIName(QualType T, raw_ostream &Out,
                                              bool NormalizeIntegers) {
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
