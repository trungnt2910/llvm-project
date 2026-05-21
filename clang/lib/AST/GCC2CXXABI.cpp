//===------- GCC2CXXABI.cpp - AST support for the legacy GCC2 C++ ABI ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides C++ AST support targeting the legacy GCC 2.x C++ ABI.
//
//===----------------------------------------------------------------------===//

#include "CXXABI.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/MangleNumberingContext.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Basic/TargetInfo.h"
#include <optional>

using namespace clang;

namespace {

class GCC2NumberingContext : public MangleNumberingContext {
  ItaniumMangleContext *Mangler;
  llvm::StringMap<unsigned> LambdaManglingNumbers;
  unsigned BlockManglingNumber = 0;
  llvm::DenseMap<const IdentifierInfo *, unsigned> VarManglingNumbers;
  llvm::DenseMap<const IdentifierInfo *, unsigned> TagManglingNumbers;

public:
  GCC2NumberingContext(ItaniumMangleContext *Mangler) : Mangler(Mangler) {}

  unsigned getManglingNumber(const CXXMethodDecl *CallOperator) override {
    const CXXRecordDecl *Lambda = CallOperator->getParent();
    assert(Lambda->isLambda());

    llvm::SmallString<128> LambdaSig;
    llvm::raw_svector_ostream Out(LambdaSig);
    Mangler->mangleLambdaSig(Lambda, Out);

    return ++LambdaManglingNumbers[LambdaSig];
  }

  unsigned getManglingNumber(const BlockDecl *BD) override {
    return ++BlockManglingNumber;
  }

  unsigned getStaticLocalNumber(const VarDecl *VD) override {
    return 0;
  }

  unsigned getManglingNumber(const VarDecl *VD, unsigned) override {
    const IdentifierInfo *Identifier = VD->getIdentifier();
    if (!Identifier)
      return 0;
    return ++VarManglingNumbers[Identifier];
  }

  unsigned getManglingNumber(const TagDecl *TD, unsigned) override {
    return ++TagManglingNumbers[TD->getIdentifier()];
  }
};

class GCC2CXXABI : public CXXABI {
private:
  std::unique_ptr<MangleContext> Mangler;
protected:
  ASTContext &Context;
  bool IsItaniumFallback = false;
public:
  GCC2CXXABI(ASTContext &Ctx)
      : Mangler(Ctx.createMangleContext()), Context(Ctx) {}

  MemberPointerInfo
  getMemberPointerInfo(const MemberPointerType *MPT) const override {
    const TargetInfo &Target = Context.getTargetInfo();
    TargetInfo::IntType PtrDiff = Target.getPtrDiffType(LangAS::Default);
    MemberPointerInfo MPI;
    MPI.Width = Target.getTypeWidth(PtrDiff);
    MPI.Align = Target.getTypeAlign(PtrDiff);
    MPI.HasPadding = false;
    if (MPT->isMemberFunctionPointer())
      MPI.Width *= 2;
    return MPI;
  }

  CallingConv getDefaultMethodCallConv(bool isVariadic) const override {
    return Context.getTargetInfo().getDefaultCallingConv();
  }

  bool isNearlyEmpty(const CXXRecordDecl *RD) const override {
    if (!RD->isDynamicClass())
      return false;

    const ASTRecordLayout &Layout = Context.getASTRecordLayout(RD);
    CharUnits PointerSize = Context.toCharUnitsFromBits(
        Context.getTargetInfo().getPointerWidth(LangAS::Default));
    return Layout.getNonVirtualSize() == PointerSize;
  }

  const CXXConstructorDecl *
  getCopyConstructorForExceptionObject(CXXRecordDecl *RD) override {
    return nullptr;
  }

  void addCopyConstructorForExceptionObject(CXXRecordDecl *RD,
                                            CXXConstructorDecl *CD) override {}

  void addTypedefNameForUnnamedTagDecl(TagDecl *TD,
                                       TypedefNameDecl *DD) override {}

  TypedefNameDecl *getTypedefNameForUnnamedTagDecl(const TagDecl *TD) override {
    return nullptr;
  }

  void addDeclaratorForUnnamedTagDecl(TagDecl *TD,
                                      DeclaratorDecl *DD) override {}

  DeclaratorDecl *getDeclaratorForUnnamedTagDecl(const TagDecl *TD) override {
    return nullptr;
  }

  std::unique_ptr<MangleNumberingContext>
  createMangleNumberingContext() const override {
    return std::make_unique<GCC2NumberingContext>(
        cast<ItaniumMangleContext>(Mangler.get()));
  }

  bool isStaticLocalVarInternal() const override { return true; }
};
}

CXXABI *clang::CreateGCC2CXXABI(ASTContext &Ctx) {
  return new GCC2CXXABI(Ctx);
}

std::unique_ptr<MangleNumberingContext>
clang::createGCC2NumberingContext(MangleContext *Mangler) {
  return std::make_unique<GCC2NumberingContext>(
      cast<ItaniumMangleContext>(Mangler));
}
