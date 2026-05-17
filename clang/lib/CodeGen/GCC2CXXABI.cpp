//===------- GCC2CXXABI.cpp - CodeGen support for Legacy GCC 2.x ABI -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides C++ CodeGen support targeting the legacy GCC 2.x C++ ABI.
//
//===----------------------------------------------------------------------===//

#include "CGCXXABI.h"
#include "CGCleanup.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGRecordLayout.h"
#include "CGVTables.h"
#include "TargetInfo.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/StmtCXX.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ConstantFold.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include <optional>


using namespace clang;
using namespace CodeGen;

namespace {

static bool isThreadWrapperReplaceable(const VarDecl *VD,
                                       CodeGen::CodeGenModule &CGM) {
  assert(!VD->isStaticLocal() && "static local VarDecls don't need wrappers!");
  return VD->getTLSKind() == VarDecl::TLS_Dynamic &&
         CGM.getTarget().getTriple().isOSDarwin();
}

static llvm::GlobalValue::LinkageTypes
getThreadLocalWrapperLinkage(const VarDecl *VD, CodeGen::CodeGenModule &CGM) {
  llvm::GlobalValue::LinkageTypes VarLinkage =
      CGM.getLLVMLinkageVarDefinition(VD);
  if (llvm::GlobalValue::isLocalLinkage(VarLinkage))
    return VarLinkage;
  if (isThreadWrapperReplaceable(VD, CGM))
    if (!llvm::GlobalVariable::isLinkOnceLinkage(VarLinkage) &&
        !llvm::GlobalVariable::isWeakODRLinkage(VarLinkage))
      return VarLinkage;
  return llvm::GlobalValue::WeakODRLinkage;
}

static llvm::Value *performTypeAdjustment(CodeGenFunction &CGF,
                                          Address InitialPtr,
                                          const CXXRecordDecl *UnadjustedClass,
                                          int64_t NonVirtualAdjustment,
                                          int64_t VirtualAdjustment,
                                          bool IsReturnAdjustment) {
  if (!NonVirtualAdjustment && !VirtualAdjustment)
    return InitialPtr.emitRawPointer(CGF);

  Address V = InitialPtr.withElementType(CGF.Int8Ty);

  if (NonVirtualAdjustment && !IsReturnAdjustment) {
    V = CGF.Builder.CreateConstInBoundsByteGEP(V,
                              CharUnits::fromQuantity(NonVirtualAdjustment));
  }

  llvm::Value *ResultPtr;
  if (VirtualAdjustment) {
    llvm::Value *VTablePtr =
        CGF.GetVTablePtr(V, CGF.Int8PtrTy, UnadjustedClass);

    llvm::Value *Offset;
    llvm::Value *OffsetPtr = CGF.Builder.CreateConstInBoundsGEP1_64(
        CGF.Int8Ty, VTablePtr, VirtualAdjustment);
    if (CGF.CGM.getLangOpts().RelativeCXXABIVTables) {
      Offset =
          CGF.Builder.CreateAlignedLoad(CGF.Int32Ty, OffsetPtr,
                                        CharUnits::fromQuantity(4));
    } else {
      llvm::Type *PtrDiffTy =
          CGF.ConvertType(CGF.getContext().getPointerDiffType());
      Offset = CGF.Builder.CreateAlignedLoad(PtrDiffTy, OffsetPtr,
                                             CGF.getPointerAlign());
    }
    ResultPtr = CGF.Builder.CreateInBoundsGEP(V.getElementType(),
                                              V.emitRawPointer(CGF), Offset);
  } else {
    ResultPtr = V.emitRawPointer(CGF);
  }

  if (NonVirtualAdjustment && IsReturnAdjustment) {
    ResultPtr = CGF.Builder.CreateConstInBoundsGEP1_64(CGF.Int8Ty, ResultPtr,
                                                       NonVirtualAdjustment);
  }

  return ResultPtr;
}

static llvm::FunctionCallee getGuardAcquireFn(CodeGenModule &CGM,
                                              llvm::PointerType *GuardPtrTy) {
  llvm::FunctionType *FTy =
    llvm::FunctionType::get(CGM.getTypes().ConvertType(CGM.getContext().IntTy),
                            GuardPtrTy, /*isVarArg=*/false);
  return CGM.CreateRuntimeFunction(
      FTy, "__cxa_guard_acquire",
      llvm::AttributeList::get(CGM.getLLVMContext(),
                               llvm::AttributeList::FunctionIndex,
                               llvm::Attribute::NoUnwind));
}

static llvm::FunctionCallee getGuardReleaseFn(CodeGenModule &CGM,
                                              llvm::PointerType *GuardPtrTy) {
  llvm::FunctionType *FTy =
    llvm::FunctionType::get(CGM.VoidTy, GuardPtrTy, /*isVarArg=*/false);
  return CGM.CreateRuntimeFunction(
      FTy, "__cxa_guard_release",
      llvm::AttributeList::get(CGM.getLLVMContext(),
                               llvm::AttributeList::FunctionIndex,
                               llvm::Attribute::NoUnwind));
}

static llvm::FunctionCallee getGuardAbortFn(CodeGenModule &CGM,
                                            llvm::PointerType *GuardPtrTy) {
  llvm::FunctionType *FTy =
    llvm::FunctionType::get(CGM.VoidTy, GuardPtrTy, /*isVarArg=*/false);
  return CGM.CreateRuntimeFunction(
      FTy, "__cxa_guard_abort",
      llvm::AttributeList::get(CGM.getLLVMContext(),
                               llvm::AttributeList::FunctionIndex,
                               llvm::Attribute::NoUnwind));
}

struct CallGuardAbort final : EHScopeStack::Cleanup {
  llvm::GlobalVariable *Guard;
  CallGuardAbort(llvm::GlobalVariable *Guard) : Guard(Guard) {}

  void Emit(CodeGenFunction &CGF, Flags flags) override {
    CGF.EmitNounwindRuntimeCall(getGuardAbortFn(CGF.CGM, Guard->getType()),
                                Guard);
  }
};

static void emitGlobalDtorWithCXAAtExit(CodeGenFunction &CGF,
                                        llvm::FunctionCallee dtor,
                                        llvm::Constant *addr, bool TLS) {
  const char *Name = TLS ? "__cxa_thread_atexit" : "__cxa_atexit";
  llvm::Type *dtorTy = CGF.DefaultPtrTy;
  auto AddrAS = addr ? addr->getType()->getPointerAddressSpace() : 0;
  auto AddrPtrTy = AddrAS ? llvm::PointerType::get(CGF.getLLVMContext(), AddrAS)
                          : CGF.Int8PtrTy;

  llvm::Constant *handle =
      CGF.CGM.CreateRuntimeVariable(CGF.Int8Ty, "__dso_handle");
  auto *GV = cast<llvm::GlobalValue>(handle->stripPointerCasts());
  GV->setVisibility(llvm::GlobalValue::HiddenVisibility);

  llvm::Type *paramTys[] = {dtorTy, AddrPtrTy, handle->getType()};
  llvm::FunctionType *atexitTy =
    llvm::FunctionType::get(CGF.IntTy, paramTys, false);

  llvm::FunctionCallee atexit = CGF.CGM.CreateRuntimeFunction(atexitTy, Name);
  if (llvm::Function *fn = dyn_cast<llvm::Function>(atexit.getCallee()))
    fn->setDoesNotThrow();

  const auto &Context = CGF.CGM.getContext();
  FunctionProtoType::ExtProtoInfo EPI(Context.getDefaultCallingConvention(
      /*IsVariadic=*/false, /*IsCXXMethod=*/false));
  QualType fnType =
      Context.getFunctionType(Context.VoidTy, {Context.VoidPtrTy}, EPI);
  llvm::Value *dtorCallee = dtor.getCallee();
  dtorCallee =
      CGF.CGM.getFunctionPointer(cast<llvm::Constant>(dtorCallee), fnType);

  if (dtorCallee->getType()->getPointerAddressSpace() != AddrAS)
    dtorCallee = CGF.performAddrSpaceCast(dtorCallee, AddrPtrTy);

  if (!addr)
    addr = llvm::Constant::getNullValue(CGF.Int8PtrTy);

  llvm::Value *args[] = {dtorCallee, addr, handle};
  CGF.EmitNounwindRuntimeCall(atexit, args);
}

class GCC2CXXABI : public CGCXXABI {
  llvm::DenseMap<const CXXRecordDecl *, llvm::GlobalVariable *> VTables;
  llvm::SmallVector<std::pair<const VarDecl *, llvm::Function *>, 8> ThreadWrappers;

protected:
  GCC2MangleContext &getMangleContext() {
    return cast<GCC2MangleContext>(CodeGen::CGCXXABI::getMangleContext());
  }

private:
  llvm::Function *getOrCreateThreadLocalWrapper(const VarDecl *VD,
                                                llvm::Value *Val) {
    SmallString<256> WrapperName;
    {
      llvm::raw_svector_ostream Out(WrapperName);
      getMangleContext().mangleItaniumThreadLocalWrapper(VD, Out);
    }
    if (llvm::Value *V = CGM.getModule().getNamedValue(WrapperName))
      return cast<llvm::Function>(V);

    QualType RetQT = VD->getType();
    if (RetQT->isReferenceType())
      RetQT = RetQT.getNonReferenceType();

    const CGFunctionInfo &FI = CGM.getTypes().arrangeBuiltinFunctionDeclaration(
        getContext().getPointerType(RetQT), FunctionArgList());

    llvm::FunctionType *FnTy = CGM.getTypes().GetFunctionType(FI);
    llvm::Function *Wrapper =
        llvm::Function::Create(FnTy, getThreadLocalWrapperLinkage(VD, CGM),
                               WrapperName.str(), &CGM.getModule());
    if (CGM.supportsCOMDAT() && Wrapper->isWeakForLinker())
      Wrapper->setComdat(CGM.getModule().getOrInsertComdat(Wrapper->getName()));

    CGM.SetLLVMFunctionAttributes(GlobalDecl(), FI, Wrapper, /*IsThunk=*/false);

    if (!Wrapper->hasLocalLinkage())
      if (!isThreadWrapperReplaceable(VD, CGM) ||
          llvm::GlobalVariable::isLinkOnceLinkage(Wrapper->getLinkage()) ||
          llvm::GlobalVariable::isWeakODRLinkage(Wrapper->getLinkage()) ||
          VD->getVisibility() == HiddenVisibility)
        Wrapper->setVisibility(llvm::GlobalValue::HiddenVisibility);

    if (isThreadWrapperReplaceable(VD, CGM)) {
      Wrapper->setCallingConv(llvm::CallingConv::CXX_FAST_TLS);
      Wrapper->addFnAttr(llvm::Attribute::NoUnwind);
    }

    ThreadWrappers.push_back({VD, Wrapper});
    return Wrapper;
  }

public:
  GCC2CXXABI(CodeGenModule &CGM) : CGCXXABI(CGM) {}

  bool isThisCompleteObject(GlobalDecl GD) const override {
    return true;
  }

  bool classifyReturnType(CGFunctionInfo &FI) const override {
    const CXXRecordDecl *RD = FI.getReturnType()->getAsCXXRecordDecl();
    if (!RD) return false;
    if (!RD->canPassInRegisters()) {
      auto Align = CGM.getContext().getTypeAlignInChars(FI.getReturnType());
      LangAS SRetAS = CGM.getTargetCodeGenInfo().getSRetAddrSpace(RD);
      unsigned AS = CGM.getContext().getTargetAddressSpace(SRetAS);
      FI.getReturnInfo() = ABIArgInfo::getIndirect(Align, AS, /*ByVal=*/false);
      return true;
    }
    return false;
  }

  RecordArgABI getRecordArgABI(const CXXRecordDecl *RD) const override {
    if (!RD->canPassInRegisters())
      return RAA_Indirect;
    return RAA_Default;
  }

  bool isVirtualOffsetNeededForVTableField(CodeGenFunction &CGF,
                                           CodeGenFunction::VPtr Vptr) override {
    return false;
  }

  bool doStructorsInitializeVPtrs(const CXXRecordDecl *VTableClass) override {
    return true;
  }

  llvm::Constant *getVTableAddressPoint(BaseSubobject Base,
                                        const CXXRecordDecl *VTableClass) override {
    const VTableLayout &VTLayout = CGM.getItaniumVTableContext().getVTableLayout(VTableClass);
    VTableLayout::AddressPointLocation AddressPoint = VTLayout.getAddressPoint(Base);
    unsigned vtableIndex = AddressPoint.VTableIndex;

    SmallString<256> Name;
    {
      llvm::raw_svector_ostream Out(Name);
      getMangleContext().mangleCXXVTable(VTableClass, Out);
    }

    if (vtableIndex > 0) {
      const CXXRecordDecl *BaseRD = Base.getBase();
      SmallString<256> BaseVTName;
      {
        llvm::raw_svector_ostream Out(BaseVTName);
        getMangleContext().mangleCXXVTable(BaseRD, Out);
      }
      Name += ".";
      Name += BaseVTName.str().substr(5);
    }

    llvm::GlobalVariable *VTable = CGM.getModule().getNamedGlobal(Name);
    if (!VTable) {
      llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
      llvm::Type *VTableTy = llvm::ArrayType::get(Int8PtrTy, VTLayout.getVTableSize(vtableIndex));
      VTable = new llvm::GlobalVariable(
          CGM.getModule(), VTableTy, /*isConstant=*/true,
          llvm::GlobalValue::ExternalLinkage, nullptr, Name);
    }

    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::Constant *Indices[] = {
      llvm::ConstantInt::get(CGM.Int32Ty, 0),
      llvm::ConstantInt::get(CGM.Int32Ty, 0)
    };
    llvm::Constant *GEP = llvm::ConstantExpr::getInBoundsGetElementPtr(
        VTable->getValueType(), VTable, Indices);
    return llvm::ConstantExpr::getBitCast(GEP, Int8PtrTy);
  }

  llvm::Value *
  getVTableAddressPointInStructor(CodeGenFunction &CGF, const CXXRecordDecl *RD,
                                  BaseSubobject Base,
                                  const CXXRecordDecl *NearestVBase) override {
    return getVTableAddressPoint(Base, RD);
  }

  llvm::GlobalVariable *getAddrOfVTable(const CXXRecordDecl *RD,
                                        CharUnits VPtrOffset) override {
    SmallString<256> Name;
    {
      llvm::raw_svector_ostream Out(Name);
      getMangleContext().mangleCXXVTable(RD, Out);
    }

    if (llvm::GlobalVariable *GV = CGM.getModule().getNamedGlobal(Name))
      return GV;

    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::Type *VTableTy = llvm::ArrayType::get(Int8PtrTy, 3);
    llvm::GlobalVariable *GV = new llvm::GlobalVariable(
        CGM.getModule(), VTableTy, /*isConstant=*/true,
        llvm::GlobalValue::ExternalLinkage, nullptr, Name);
    return GV;
  }

  CGCallee getVirtualFunctionPointer(CodeGenFunction &CGF, GlobalDecl GD,
                                     Address This, llvm::Type *Ty,
                                     SourceLocation Loc) override {
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    auto *MethodDecl = cast<CXXMethodDecl>(GD.getDecl());
    const CXXRecordDecl *RD = MethodDecl->getParent();
    llvm::Value *VTable = CGF.GetVTablePtr(This, Int8PtrTy, RD);
    if (isa<CXXDestructorDecl>(MethodDecl))
      GD = GD.getWithDtorType(Dtor_Base);
    uint64_t Index = CGM.getItaniumVTableContext().getMethodVTableIndex(GD);

    llvm::Value *VFuncPtr = CGF.Builder.CreateConstInBoundsGEP1_64(
        Int8PtrTy, VTable, Index);
    llvm::Value *VFunc = CGF.Builder.CreateAlignedLoad(
        Int8PtrTy, VFuncPtr, CGF.getPointerAlign());
    return CGCallee(GD, VFunc, CGPointerAuthInfo());
  }

  std::pair<llvm::Value *, const CXXRecordDecl *>
  LoadVTablePtr(CodeGenFunction &CGF, Address This,
                const CXXRecordDecl *RD) override {
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::Value *VPtr = CGF.GetVTablePtr(This, Int8PtrTy, RD);
    return {VPtr, RD};
  }

  llvm::Value *
  EmitVirtualDestructorCall(CodeGenFunction &CGF, const CXXDestructorDecl *Dtor,
                            CXXDtorType DtorType, Address This,
                            DeleteOrMemberCallExpr E,
                            llvm::CallBase **CallOrInvoke) override {
    auto *CE = E.dyn_cast<const CXXMemberCallExpr *>();
    auto *D = E.dyn_cast<const CXXDeleteExpr *>();
    GlobalDecl GD(Dtor, DtorType);
    const CGFunctionInfo *FInfo = &CGM.getTypes().arrangeCXXStructorDeclaration(GD);
    llvm::FunctionType *Ty = CGF.CGM.getTypes().GetFunctionType(*FInfo);
    CGCallee Callee = getVirtualFunctionPointer(CGF, GD, This, Ty, CE ? CE->getBeginLoc() : D->getBeginLoc());
    QualType ThisTy = CE ? CE->getObjectType() : D->getDestroyedType();
    llvm::Value *InChrg = getCXXDestructorImplicitParam(CGF, Dtor, DtorType, false, false);
    QualType InChrgTy = getContext().IntTy;

    llvm::Value *ThisPtr = This.emitRawPointer(CGF);
    const CXXRecordDecl *RD = Dtor->getParent();
    const ASTRecordLayout &Layout = getContext().getASTRecordLayout(RD);
    if (!Layout.hasOwnVFPtr() && RD->getNumVBases() > 0) {
      const CXXRecordDecl *VBase = RD->vbases_begin()->getType()->getAsCXXRecordDecl();
      llvm::Value *VBaseOffset = GetVirtualBaseClassOffset(CGF, This, RD, VBase);
      ThisPtr = CGF.Builder.CreateInBoundsGEP(CGF.Int8Ty, ThisPtr, VBaseOffset);
    }

    CallArgList Args;
    Args.add(RValue::get(ThisPtr), CGF.getTypes().DeriveThisType(Dtor->getParent(), Dtor));
    if (Dtor->getParent()->getNumVBases() != 0 && hasPolymorphicVBases(Dtor->getParent())) {
      llvm::Value *VList = llvm::ConstantPointerNull::get(CGM.Int8PtrTy);
      Args.add(RValue::get(VList), getContext().VoidPtrTy);
    }
    Args.add(RValue::get(InChrg), InChrgTy);
    CGF.EmitCall(*FInfo, Callee, ReturnValueSlot(), Args, CallOrInvoke, false, CE ? CE->getExprLoc() : SourceLocation{});
    return nullptr;
  }

  void emitVirtualObjectDelete(CodeGenFunction &CGF, const CXXDeleteExpr *DE,
                               Address Ptr, QualType ElementType,
                               const CXXDestructorDecl *Dtor) override {
    EmitVirtualDestructorCall(CGF, Dtor, Dtor_Deleting, Ptr, DE, nullptr);
  }

  void emitRethrow(CodeGenFunction &CGF, bool isNoReturn) override {
    bool IsSJLJ = CGM.getCodeGenOpts().hasSjLjExceptions();
    const char *ThrowName = IsSJLJ ? "__sjthrow" : "__throw";
    llvm::FunctionCallee ThrowFn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(CGF.VoidTy, false), ThrowName);
    CGF.EmitRuntimeCallOrInvoke(ThrowFn);
    if (isNoReturn)
      CGF.Builder.CreateUnreachable();
  }

  void emitThrow(CodeGenFunction &CGF, const CXXThrowExpr *E) override {
    if (!E->getSubExpr()) {
      emitRethrow(CGF, true);
      return;
    }

    QualType ThrowType = E->getSubExpr()->getType();
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;

    llvm::Value *Alloc;
    if (ThrowType->isPointerType() || ThrowType->isReferenceType()) {
      Alloc = CGF.EmitScalarExpr(E->getSubExpr());
      Alloc = CGF.Builder.CreateBitCast(Alloc, Int8PtrTy);
    } else {
      llvm::FunctionCallee AllocFn = CGM.CreateRuntimeFunction(
          llvm::FunctionType::get(Int8PtrTy, {CGM.Int32Ty}, false), "__eh_alloc");

      CharUnits Size = CGM.getContext().getTypeSizeInChars(ThrowType);
      Alloc = CGF.EmitRuntimeCall(
          AllocFn, {llvm::ConstantInt::get(CGM.Int32Ty, Size.getQuantity())});

      Address AllocAddr(Alloc, Int8PtrTy, CGM.getContext().getTypeAlignInChars(ThrowType));
      CGF.EmitAnyExprToMem(E->getSubExpr(), AllocAddr, ThrowType.getQualifiers(),
                           /*IsInit=*/true);
    }

    llvm::Constant *RTTIFn = getAddrOfRTTIFunction(ThrowType);
    llvm::Constant *DtorPtr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(Int8PtrTy));
    if (const auto *RD = ThrowType->getAsCXXRecordDecl()) {
      if (CXXDestructorDecl *Dtor = RD->getDestructor()) {
        if (!Dtor->isTrivial()) {
          GlobalDecl GD(Dtor, Dtor_Base);
          DtorPtr = CGM.getAddrOfCXXStructor(GD, nullptr, nullptr, true);
          DtorPtr = llvm::ConstantExpr::getBitCast(DtorPtr, Int8PtrTy);
        }
      }
    }

    llvm::FunctionCallee PushFn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(CGF.VoidTy, {Int8PtrTy, Int8PtrTy, Int8PtrTy}, false),
        "__cp_push_exception");

    llvm::FunctionType *RTTIFTy = llvm::FunctionType::get(Int8PtrTy, false);
    llvm::FunctionCallee RTTICallee({RTTIFTy, RTTIFn});
    llvm::Value *RTTIVal = CGF.EmitRuntimeCallOrInvoke(RTTICallee);

    CGF.EmitRuntimeCallOrInvoke(PushFn, {Alloc, RTTIVal, DtorPtr});

    bool IsSJLJ = CGM.getCodeGenOpts().hasSjLjExceptions();
    const char *ThrowName = IsSJLJ ? "__sjthrow" : "__throw";
    llvm::FunctionCallee ThrowFn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(CGF.VoidTy, false), ThrowName);
    CGF.EmitRuntimeCallOrInvoke(ThrowFn);
    CGF.Builder.CreateUnreachable();
  }

  struct CallPopException final : EHScopeStack::Cleanup {
    CallPopException(llvm::Value *Exn) : Exn(Exn) {}
    llvm::Value *Exn;

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      llvm::FunctionCallee PopFn = CGF.CGM.CreateRuntimeFunction(
          llvm::FunctionType::get(CGF.VoidTy, {CGF.CGM.Int8PtrTy}, false),
          "__cp_pop_exception");
      CGF.EmitRuntimeCallOrInvoke(PopFn, {Exn});
    }
  };

  void emitBeginCatch(CodeGenFunction &CGF, const CXXCatchStmt *C) override {
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::FunctionCallee StartFn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(Int8PtrTy, false), "__start_cp_handler");
    llvm::Value *Handler = CGF.EmitRuntimeCall(StartFn);

    CGF.EHStack.pushCleanup<CallPopException>(NormalAndEHCleanup, Handler);

    if (C->getExceptionDecl()) {
      const VarDecl *VD = C->getExceptionDecl();
      CodeGenFunction::AutoVarEmission var = CGF.EmitAutoVarAlloca(*VD);
      Address ParamAddr = var.getObjectAddress(CGF);
      QualType CatchType = VD->getType();

      llvm::Value *ValuePtr = CGF.Builder.CreateConstInBoundsGEP1_64(Int8PtrTy, Handler, 2);
      llvm::Value *Exn = CGF.Builder.CreateAlignedLoad(Int8PtrTy, ValuePtr, CGF.getPointerAlign());

      if (CatchType->isReferenceType()) {
        CGF.Builder.CreateStore(Exn, ParamAddr);
      } else if (CatchType->isPointerType()) {
        llvm::Value *PtrVal = CGF.Builder.CreateBitCast(Exn, CGF.ConvertType(CatchType));
        CGF.Builder.CreateStore(PtrVal, ParamAddr);
      } else {
        CGF.EmitAggregateCopy(CGF.MakeAddrLValue(ParamAddr, CatchType),
                              CGF.MakeAddrLValue(Address(Exn, Int8PtrTy, CGF.getPointerAlign()), CatchType),
                              CatchType, AggValueSlot::DoesNotOverlap);
      }
      CGF.EmitAutoVarCleanups(var);
    }
  }

  llvm::Constant *getAddrOfRTTIFunction(QualType Ty) {
    getAddrOfRTTIDescriptor(Ty);
    SmallString<256> FnName;
    {
      llvm::raw_svector_ostream Out(FnName);
      getMangleContext().mangleCXXRTTI(Ty, Out);
    }
    if (llvm::Function *F = CGM.getModule().getFunction(FnName))
      return F;
    llvm::FunctionType *FTy = llvm::FunctionType::get(CGM.Int8PtrTy, false);
    return llvm::Function::Create(FTy, llvm::GlobalValue::ExternalLinkage, FnName.str(), &CGM.getModule());
  }

  llvm::Constant *getAddrOfRTTIDescriptor(QualType Ty) override {
    SmallString<256> Name;
    {
      llvm::raw_svector_ostream Out(Name);
      getMangleContext().mangleCXXRTTIName(Ty, Out);
    }
    if (llvm::GlobalVariable *GV = CGM.getModule().getNamedGlobal(Name))
      return GV;

    SmallString<256> FnName;
    {
      llvm::raw_svector_ostream Out(FnName);
      getMangleContext().mangleCXXRTTI(Ty, Out);
    }

    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;

    SmallString<256> TypeNameStr;
    {
      llvm::raw_svector_ostream Out(TypeNameStr);
      getMangleContext().mangleCanonicalTypeName(Ty, Out);
    }
    llvm::Constant *TypeNameConst = CGM.GetAddrOfConstantCString(TypeNameStr.str().str()).getPointer();

    StringRef VTableName = "__vt_16__user_type_info";
    llvm::Constant *ThirdElem = nullptr;
    llvm::Constant *FourthElem = nullptr;
    llvm::Constant *ThirdFn = nullptr;
    SmallVector<llvm::Constant *, 4> BaseTfs;
    llvm::Type *RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);

    QualType CanTy = Ty.getCanonicalType();
    if (CanTy.isConstQualified() || CanTy.isVolatileQualified()) {
      VTableName = "__vt_16__attr_type_info";
      QualType UnqualTy = CanTy.getUnqualifiedType();
      ThirdElem = getAddrOfRTTIDescriptor(UnqualTy);
      ThirdFn = getAddrOfRTTIFunction(UnqualTy);
      unsigned CVR = CanTy.getCVRQualifiers();
      unsigned AttrVal = 0;
      if (CVR & Qualifiers::Const) AttrVal |= 1;
      if (CVR & Qualifiers::Volatile) AttrVal |= 2;
      FourthElem = llvm::ConstantInt::get(CGM.Int32Ty, AttrVal);
      RTTITy = llvm::StructType::get(CGM.getLLVMContext(), {Int8PtrTy, Int8PtrTy, Int8PtrTy, CGM.Int32Ty});
    } else if (Ty->isPointerType() || Ty->isReferenceType()) {
      VTableName = "__vt_19__pointer_type_info";
      QualType PointeeTy = Ty->isPointerType() ? Ty->getPointeeType() : Ty.getNonReferenceType();
      ThirdElem = getAddrOfRTTIDescriptor(PointeeTy);
      ThirdFn = getAddrOfRTTIFunction(PointeeTy);
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 3);
    } else if (Ty->isMemberFunctionPointerType()) {
      VTableName = "__vt_16__ptmf_type_info";
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
    } else if (Ty->isMemberDataPointerType()) {
      VTableName = "__vt_16__ptmd_type_info";
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
    } else if (Ty->isArrayType()) {
      VTableName = "__vt_17__array_type_info";
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
    } else if (Ty->isFunctionType()) {
      VTableName = "__vt_16__func_type_info";
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
    } else if (Ty->isBuiltinType()) {
      VTableName = "__vt_19__builtin_type_info";
      RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
    } else if (const auto *RD = Ty->getAsCXXRecordDecl()) {
      if (RD->getNumBases() == 0) {
        VTableName = "__vt_16__user_type_info";
        RTTITy = llvm::ArrayType::get(Int8PtrTy, 2);
      } else if (RD->getNumBases() == 1 && !RD->bases_begin()->isVirtual() && RD->bases_begin()->getAccessSpecifier() == AS_public) {
        VTableName = "__vt_14__si_type_info";
        ThirdElem = getAddrOfRTTIDescriptor(RD->bases_begin()->getType());
        ThirdFn = getAddrOfRTTIFunction(RD->bases_begin()->getType());
        RTTITy = llvm::ArrayType::get(Int8PtrTy, 3);
      } else {
        VTableName = "__vt_17__class_type_info";
        SmallVector<llvm::Constant *, 4> BaseInfos;
        llvm::StructType *BaseInfoTy = llvm::StructType::get(CGM.getLLVMContext(), {Int8PtrTy, CGM.Int32Ty});
        const ASTRecordLayout &Layout = getContext().getASTRecordLayout(RD);
        for (const auto &Base : RD->bases()) {
          llvm::Constant *BaseRTTI = getAddrOfRTTIDescriptor(Base.getType());
          llvm::Constant *BaseTf = getAddrOfRTTIFunction(Base.getType());
          BaseTfs.push_back(BaseTf);
          uint32_t Offset;
          if (Base.isVirtual())
            Offset = Layout.getVBaseClassOffset(Base.getType()->getAsCXXRecordDecl()).getQuantity();
          else
            Offset = Layout.getBaseClassOffset(Base.getType()->getAsCXXRecordDecl()).getQuantity();
          uint32_t Flags = Offset;
          if (Base.isVirtual()) Flags |= (1 << 29);
          if (Base.getAccessSpecifier() == AS_public) Flags |= (1 << 30);
          else if (Base.getAccessSpecifier() == AS_protected) Flags |= (2 << 30);
          else Flags |= (3 << 30);
          llvm::Constant *BaseInfoInit = llvm::ConstantStruct::get(BaseInfoTy, {
            BaseRTTI, llvm::ConstantInt::get(CGM.Int32Ty, Flags)
          });
          BaseInfos.push_back(BaseInfoInit);
        }
        llvm::ArrayType *BaseInfoArrayTy = llvm::ArrayType::get(BaseInfoTy, BaseInfos.size());
        llvm::Constant *BaseInfoArrayInit = llvm::ConstantArray::get(BaseInfoArrayTy, BaseInfos);
        llvm::GlobalVariable *BaseListGV = new llvm::GlobalVariable(
            CGM.getModule(), BaseInfoArrayTy, /*isConstant=*/true,
            llvm::GlobalValue::PrivateLinkage, BaseInfoArrayInit, Name.str().str() + ".base_list");
        BaseListGV->setAlignment(llvm::Align(4));
        ThirdElem = llvm::ConstantExpr::getBitCast(BaseListGV, Int8PtrTy);
        FourthElem = llvm::ConstantInt::get(CGM.Int32Ty, BaseInfos.size());
        RTTITy = llvm::StructType::get(CGM.getLLVMContext(), {Int8PtrTy, Int8PtrTy, Int8PtrTy, CGM.Int32Ty});
      }
    }

    llvm::GlobalVariable *VTable = CGM.getModule().getNamedGlobal(VTableName);
    if (!VTable) {
      llvm::Type *VTableTy = llvm::ArrayType::get(Int8PtrTy, 4);
      VTable = new llvm::GlobalVariable(
          CGM.getModule(), VTableTy, /*isConstant=*/true,
          llvm::GlobalValue::ExternalLinkage, nullptr, VTableName);
    }
    llvm::Constant *Indices[] = {
      llvm::ConstantInt::get(CGM.Int32Ty, 0),
      llvm::ConstantInt::get(CGM.Int32Ty, 0)
    };
    llvm::Constant *VPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        VTable->getValueType(), VTable, Indices);
    VPtr = llvm::ConstantExpr::getBitCast(VPtr, Int8PtrTy);

    llvm::Constant *Init;
    if (FourthElem) {
      llvm::Constant *InitElems[] = {
        llvm::ConstantExpr::getBitCast(TypeNameConst, Int8PtrTy),
        VPtr,
        ThirdElem,
        FourthElem
      };
      Init = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(RTTITy), InitElems);
    } else if (ThirdElem) {
      llvm::Constant *InitElems[] = {
        llvm::ConstantExpr::getBitCast(TypeNameConst, Int8PtrTy),
        VPtr,
        ThirdElem
      };
      Init = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(RTTITy), InitElems);
    } else {
      llvm::Constant *InitElems[] = {
        llvm::ConstantExpr::getBitCast(TypeNameConst, Int8PtrTy),
        VPtr
      };
      Init = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(RTTITy), InitElems);
    }

    llvm::GlobalVariable *GV = new llvm::GlobalVariable(
        CGM.getModule(), RTTITy, /*isConstant=*/false,
        llvm::GlobalValue::WeakODRLinkage, Init, Name);
    GV->setAlignment(llvm::Align(4));

    llvm::FunctionType *FTy = llvm::FunctionType::get(Int8PtrTy, false);
    llvm::Function *F = llvm::Function::Create(FTy, llvm::GlobalValue::WeakODRLinkage, FnName.str(), &CGM.getModule());
    llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(CGM.getLLVMContext(), "entry", F);
    llvm::BasicBlock *InitBB = llvm::BasicBlock::Create(CGM.getLLVMContext(), "init", F);
    llvm::BasicBlock *DoneBB = llvm::BasicBlock::Create(CGM.getLLVMContext(), "done", F);

    CGBuilderTy Builder(CGM, EntryBB);
    llvm::Value *ThirdVal = ThirdElem;
    if (ThirdFn)
      ThirdVal = Builder.CreateCall(llvm::FunctionType::get(Int8PtrTy, false), ThirdFn);
    for (llvm::Constant *BaseTf : BaseTfs)
      Builder.CreateCall(llvm::FunctionType::get(Int8PtrTy, false), BaseTf);

    llvm::Value *NameFieldPtr = Builder.CreateConstInBoundsGEP2_32(RTTITy, GV, 0, 0);
    llvm::Value *NameField = Builder.CreateAlignedLoad(Int8PtrTy, NameFieldPtr, CharUnits::fromQuantity(4));
    llvm::Value *IsNull = Builder.CreateIsNull(NameField);
    Builder.CreateCondBr(IsNull, InitBB, DoneBB);

    Builder.SetInsertPoint(InitBB);
    Builder.CreateAlignedStore(llvm::ConstantExpr::getBitCast(TypeNameConst, Int8PtrTy), NameFieldPtr, CharUnits::fromQuantity(4));
    llvm::Value *VPtrFieldPtr = Builder.CreateConstInBoundsGEP2_32(RTTITy, GV, 0, 1);
    Builder.CreateAlignedStore(VPtr, VPtrFieldPtr, CharUnits::fromQuantity(4));
    if (FourthElem) {
      llvm::Value *ThirdFieldPtr = Builder.CreateConstInBoundsGEP2_32(RTTITy, GV, 0, 2);
      Builder.CreateAlignedStore(ThirdVal, ThirdFieldPtr, CharUnits::fromQuantity(4));
      llvm::Value *FourthFieldPtr = Builder.CreateConstInBoundsGEP2_32(RTTITy, GV, 0, 3);
      Builder.CreateAlignedStore(FourthElem, FourthFieldPtr, CharUnits::fromQuantity(4));
    } else if (ThirdElem) {
      llvm::Value *ThirdFieldPtr = Builder.CreateConstInBoundsGEP2_32(RTTITy, GV, 0, 2);
      Builder.CreateAlignedStore(ThirdVal, ThirdFieldPtr, CharUnits::fromQuantity(4));
    }
    Builder.CreateBr(DoneBB);

    Builder.SetInsertPoint(DoneBB);
    Builder.CreateRet(GV);

    return GV;
  }

  CatchTypeInfo
  getAddrOfCXXCatchHandlerType(QualType Ty, QualType CatchHandlerType) override {
    return CatchTypeInfo{getAddrOfRTTIFunction(CatchHandlerType.getNonReferenceType()), 0};
  }

  bool shouldTypeidBeNullChecked(QualType SrcRecordTy) override {
    return true;
  }
  void EmitBadTypeidCall(CodeGenFunction &CGF) override {
    llvm::FunctionCallee Fn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(CGF.VoidTy, false), "__throw_bad_typeid");
    CGF.EmitRuntimeCallOrInvoke(Fn);
  }
  llvm::Value *EmitStaticTypeid(CodeGenFunction &CGF, QualType Ty,
                                llvm::Type *StdTypeInfoPtrTy) override {
    llvm::Constant *RTTIFn = getAddrOfRTTIFunction(Ty);
    llvm::FunctionType *FTy = llvm::FunctionType::get(CGM.Int8PtrTy, false);
    llvm::Value *RTTI = CGF.EmitCallOrInvoke({FTy, RTTIFn}, {});
    return CGF.Builder.CreateBitCast(RTTI, StdTypeInfoPtrTy);
  }
  llvm::Value *EmitTypeid(CodeGenFunction &CGF, QualType SrcRecordTy,
                          Address ThisPtr, llvm::Type *StdTypeInfoPtrTy) override {
    auto *ClassDecl = SrcRecordTy->getAsCXXRecordDecl();
    llvm::Value *VTable = CGF.GetVTablePtr(ThisPtr, CGM.Int8PtrTy, ClassDecl);
    llvm::Value *RTTIPtr = CGF.Builder.CreateConstInBoundsGEP1_64(
        CGM.Int8PtrTy, VTable, 1);
    llvm::Value *RTTIFnPtr = CGF.Builder.CreateAlignedLoad(
        CGM.Int8PtrTy, RTTIPtr, CGF.getPointerAlign());
    llvm::FunctionType *FTy = llvm::FunctionType::get(CGM.Int8PtrTy, false);
    llvm::Value *RTTI = CGF.EmitCallOrInvoke({FTy, RTTIFnPtr}, {});
    return CGF.Builder.CreateBitCast(RTTI, StdTypeInfoPtrTy);
  }
  bool shouldDynamicCastCallBeNullChecked(bool SrcIsPtr,
                                          QualType SrcRecordTy) override {
    return SrcIsPtr;
  }
  bool shouldEmitExactDynamicCast(QualType DestRecordTy) override {
    return false;
  }
  llvm::Value *emitDynamicCastCall(CodeGenFunction &CGF, Address Value,
                                   QualType SrcRecordTy, QualType DestTy,
                                   QualType DestRecordTy,
                                   llvm::BasicBlock *CastEnd) override {
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::Type *Args[6] = { Int8PtrTy, Int8PtrTy, CGM.Int32Ty, Int8PtrTy, Int8PtrTy, Int8PtrTy };
    llvm::FunctionType *FTy = llvm::FunctionType::get(Int8PtrTy, Args, false);
    llvm::FunctionCallee DynCastFn = CGM.CreateRuntimeFunction(FTy, "__dynamic_cast");

    auto *SrcDecl = SrcRecordTy->getAsCXXRecordDecl();
    llvm::Value *VTable = CGF.GetVTablePtr(Value, Int8PtrTy, SrcDecl);
    llvm::Value *RTTIPtr = CGF.Builder.CreateConstInBoundsGEP1_64(Int8PtrTy, VTable, 1);
    llvm::Value *From = CGF.Builder.CreateAlignedLoad(Int8PtrTy, RTTIPtr, CGF.getPointerAlign());
    llvm::Constant *To = getAddrOfRTTIFunction(DestRecordTy);
    llvm::Value *RequirePublic = llvm::ConstantInt::get(CGM.Int32Ty, 1);

    llvm::Value *OffsetToTopPtr = CGF.Builder.CreateConstInBoundsGEP1_64(Int8PtrTy, VTable, 0);
    llvm::Value *OffsetToTop = CGF.Builder.CreateAlignedLoad(Int8PtrTy, OffsetToTopPtr, CGF.getPointerAlign());
    llvm::Value *Address = CGF.Builder.CreateInBoundsGEP(CGF.Int8Ty, CGF.Builder.CreateBitCast(Value.emitRawPointer(CGF), Int8PtrTy), CGF.Builder.CreatePtrToInt(OffsetToTop, CGM.PtrDiffTy));

    llvm::Constant *Sub = getAddrOfRTTIFunction(SrcRecordTy);
    llvm::Value *Subptr = CGF.Builder.CreateBitCast(Value.emitRawPointer(CGF), Int8PtrTy);

    llvm::Value *Res = CGF.EmitRuntimeCallOrInvoke(DynCastFn, {From, To, RequirePublic, Address, Sub, Subptr});
    return CGF.Builder.CreateBitCast(Res, CGF.ConvertType(DestTy));
  }
  llvm::Value *emitDynamicCastToVoid(CodeGenFunction &CGF, Address Value,
                                     QualType SrcRecordTy) override {
    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    auto *SrcDecl = SrcRecordTy->getAsCXXRecordDecl();
    llvm::Value *VTable = CGF.GetVTablePtr(Value, Int8PtrTy, SrcDecl);
    llvm::Value *OffsetToTopPtr = CGF.Builder.CreateConstInBoundsGEP1_64(Int8PtrTy, VTable, 0);
    llvm::Value *OffsetToTop = CGF.Builder.CreateAlignedLoad(Int8PtrTy, OffsetToTopPtr, CGF.getPointerAlign());
    llvm::Value *Res = CGF.Builder.CreateInBoundsGEP(CGF.Int8Ty, CGF.Builder.CreateBitCast(Value.emitRawPointer(CGF), Int8PtrTy), CGF.Builder.CreatePtrToInt(OffsetToTop, CGM.PtrDiffTy));
    return Res;
  }
  std::optional<ExactDynamicCastInfo>
  getExactDynamicCastInfo(QualType SrcRecordTy, QualType DestTy,
                          QualType DestRecordTy) override {
    return std::nullopt;
  }
  llvm::Value *emitExactDynamicCast(
      CodeGenFunction &CGF, Address Value, QualType SrcRecordTy,
      QualType DestTy, QualType DestRecordTy,
      const ExactDynamicCastInfo &CastInfo, llvm::BasicBlock *CastSuccess,
      llvm::BasicBlock *CastFail) override {
    return nullptr;
  }
  bool EmitBadCastCall(CodeGenFunction &CGF) override {
    llvm::FunctionCallee Fn = CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(CGF.VoidTy, false), "__bad_cast");
    CGF.EmitRuntimeCallOrInvoke(Fn);
    return true;
  }

  llvm::Type *ConvertMemberPointerType(const MemberPointerType *MPT) override {
    if (MPT->isMemberDataPointer())
      return CGM.PtrDiffTy;
    return llvm::StructType::get(CGM.Int16Ty, CGM.Int16Ty, CGM.Int8PtrTy);
  }

  llvm::Constant *EmitNullMemberPointer(const MemberPointerType *MPT) override {
    if (MPT->isMemberDataPointer())
      return llvm::ConstantInt::get(CGM.PtrDiffTy, 0);

    llvm::Constant *Values[3] = {
      llvm::ConstantInt::get(CGM.Int16Ty, 0),
      llvm::ConstantInt::get(CGM.Int16Ty, 0),
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(CGM.Int8PtrTy))
    };
    return llvm::ConstantStruct::getAnon(Values);
  }

  bool isZeroInitializable(const MemberPointerType *MPT) override {
    return true;
  }

  llvm::Constant *EmitMemberDataPointer(const MemberPointerType *MPT, CharUnits offset) override {
    return llvm::ConstantInt::get(CGM.PtrDiffTy, offset.getQuantity() + 1);
  }

  static CharUnits getOffsetToBase(const CXXRecordDecl *Derived, const CXXRecordDecl *Base, ASTContext &Context) {
    if (Derived == Base)
      return CharUnits::Zero();
    for (const auto &B : Derived->bases()) {
      const CXXRecordDecl *BaseDecl = B.getType()->getAsCXXRecordDecl();
      if (BaseDecl == Base)
        return Context.getASTRecordLayout(Derived).getBaseClassOffset(BaseDecl);
      if (BaseDecl->isDerivedFrom(Base))
        return Context.getASTRecordLayout(Derived).getBaseClassOffset(BaseDecl) + getOffsetToBase(BaseDecl, Base, Context);
    }
    return CharUnits::Zero();
  }

  static const CXXMethodDecl *getOriginalMethod(const CXXMethodDecl *MD) {
    MD = MD->getCanonicalDecl();
    while (MD->size_overridden_methods() > 0) {
      MD = (*MD->overridden_methods().begin())->getCanonicalDecl();
    }
    return MD;
  }

  llvm::Constant *BuildMemberPointer(const CXXMethodDecl *MD, CharUnits ThisAdjustment) {
    assert(MD->isInstance() && "Member function must not be static!");
    llvm::Constant *Values[3];
    if (MD->isVirtual()) {
      const CXXMethodDecl *OrigMD = getOriginalMethod(MD);
      const CXXRecordDecl *OrigClass = OrigMD->getParent();
      CharUnits BaseOffset = getOffsetToBase(MD->getParent(), OrigClass, getContext());

      uint64_t Index = CGM.getItaniumVTableContext().getMethodVTableIndex(OrigMD);
      Values[0] = llvm::ConstantInt::get(CGM.Int16Ty, (ThisAdjustment + BaseOffset).getQuantity());
      Values[1] = llvm::ConstantInt::get(CGM.Int16Ty, Index + 1);
      llvm::Constant *Delta2Val = llvm::ConstantInt::get(CGM.Int32Ty, (ThisAdjustment + BaseOffset).getQuantity());
      Values[2] = llvm::ConstantExpr::getIntToPtr(Delta2Val, CGM.Int8PtrTy);
    } else {
      const FunctionProtoType *FPT = MD->getType()->castAs<FunctionProtoType>();
      llvm::Type *Ty;
      if (CGM.getTypes().isFuncTypeConvertible(FPT))
        Ty = CGM.getTypes().GetFunctionType(CGM.getTypes().arrangeCXXMethodDeclaration(MD));
      else
        Ty = CGM.PtrDiffTy;
      llvm::Constant *addr = CGM.getMemberFunctionPointer(MD, Ty);
      Values[0] = llvm::ConstantInt::get(CGM.Int16Ty, ThisAdjustment.getQuantity());
      Values[1] = llvm::ConstantInt::get(CGM.Int16Ty, -1ULL, /*isSigned=*/true);
      Values[2] = llvm::ConstantExpr::getBitCast(addr, CGM.Int8PtrTy);
    }
    return llvm::ConstantStruct::getAnon(Values);
  }


  llvm::Constant *EmitMemberFunctionPointer(const CXXMethodDecl *MD) override {
    return BuildMemberPointer(MD, CharUnits::Zero());
  }

  llvm::Constant *EmitMemberPointer(const APValue &MP, QualType MPType) override {
    const MemberPointerType *MPT = MPType->castAs<MemberPointerType>();
    const ValueDecl *MPD = MP.getMemberPointerDecl();
    if (!MPD)
      return EmitNullMemberPointer(MPT);

    CharUnits ThisAdjustment = getContext().getMemberPointerPathAdjustment(MP);
    if (const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(MPD)) {
      return BuildMemberPointer(MD, ThisAdjustment);
    }

    getContext().recordMemberDataPointerEvaluation(MPD);
    CharUnits FieldOffset = getContext().toCharUnitsFromBits(getContext().getFieldOffset(MPD));
    return EmitMemberDataPointer(MPT, ThisAdjustment + FieldOffset);
  }

  llvm::Value *EmitMemberDataPointerAddress(CodeGenFunction &CGF, const Expr *E, Address Base, llvm::Value *MemPtr, const MemberPointerType *MPT, bool IsInBounds) override {
    assert(MemPtr->getType() == CGM.PtrDiffTy);
    CGBuilderTy &Builder = CGF.Builder;
    llvm::Value *Offset = Builder.CreateSub(MemPtr, llvm::ConstantInt::get(CGM.PtrDiffTy, 1));
    llvm::Value *BaseAddr = Base.emitRawPointer(CGF);
    return Builder.CreateGEP(CGF.Int8Ty, BaseAddr, Offset, "memptr.offset",
                             IsInBounds ? llvm::GEPNoWrapFlags::inBounds() : llvm::GEPNoWrapFlags::none());
  }

  llvm::Value *EmitMemberPointerIsNotNull(CodeGenFunction &CGF, llvm::Value *MemPtr, const MemberPointerType *MPT) override {
    CGBuilderTy &Builder = CGF.Builder;
    if (MPT->isMemberDataPointer()) {
      llvm::Value *Zero = llvm::ConstantInt::get(MemPtr->getType(), 0);
      return Builder.CreateICmpNE(MemPtr, Zero, "memptr.tobool");
    }
    llvm::Value *Index = Builder.CreateExtractValue(MemPtr, 1, "memptr.index");
    llvm::Value *Zero = llvm::ConstantInt::get(Index->getType(), 0);
    return Builder.CreateICmpNE(Index, Zero, "memptr.tobool");
  }

  llvm::Value *EmitMemberPointerComparison(CodeGenFunction &CGF, llvm::Value *L, llvm::Value *R, const MemberPointerType *MPT, bool Inequality) override {
    CGBuilderTy &Builder = CGF.Builder;
    llvm::ICmpInst::Predicate Eq = Inequality ? llvm::ICmpInst::ICMP_NE : llvm::ICmpInst::ICMP_EQ;
    if (MPT->isMemberDataPointer())
      return Builder.CreateICmp(Eq, L, R);

    llvm::Value *LIndex = Builder.CreateExtractValue(L, 1, "lhs.index");
    llvm::Value *RIndex = Builder.CreateExtractValue(R, 1, "rhs.index");
    llvm::Value *IndexEq = Builder.CreateICmpEQ(LIndex, RIndex, "index.eq");

    llvm::Value *NegOne = llvm::ConstantInt::get(LIndex->getType(), -1ULL, /*isSigned=*/true);
    llvm::Value *RIndexNotNegOne = Builder.CreateICmpNE(RIndex, NegOne, "rindex.ne.negone");

    llvm::Value *LPfnVal = Builder.CreateExtractValue(L, 2, "lhs.pfn");
    llvm::Value *RPfnVal = Builder.CreateExtractValue(R, 2, "rhs.pfn");
    llvm::Value *LDelta2 = Builder.CreateTrunc(Builder.CreatePtrToInt(LPfnVal, CGM.Int32Ty), CGM.Int16Ty);
    llvm::Value *RDelta2 = Builder.CreateTrunc(Builder.CreatePtrToInt(RPfnVal, CGM.Int32Ty), CGM.Int16Ty);
    llvm::Value *Delta2Eq = Builder.CreateICmpEQ(LDelta2, RDelta2, "delta2.eq");

    llvm::Value *VirtEq = Builder.CreateAnd(RIndexNotNegOne, Delta2Eq, "virt.eq");
    llvm::Value *PfnEq = Builder.CreateICmpEQ(LPfnVal, RPfnVal, "pfn.eq");
    llvm::Value *UnionEq = Builder.CreateOr(VirtEq, PfnEq, "union.eq");

    llvm::Value *Result = Builder.CreateAnd(IndexEq, UnionEq, "memptr.eq");
    if (Inequality)
      Result = Builder.CreateNot(Result, "memptr.ne");
    return Result;
  }

  llvm::Value *EmitMemberPointerConversion(CodeGenFunction &CGF, const CastExpr *E, llvm::Value *Src) override {
    if (isa<llvm::Constant>(Src))
      return EmitMemberPointerConversion(E, cast<llvm::Constant>(Src));

    assert(E->getCastKind() == CK_DerivedToBaseMemberPointer ||
           E->getCastKind() == CK_BaseToDerivedMemberPointer ||
           E->getCastKind() == CK_ReinterpretMemberPointer);

    if (E->getCastKind() == CK_ReinterpretMemberPointer)
      return Src;

    llvm::Constant *Adj = getMemberPointerAdjustment(E);
    if (!Adj)
      return Src;

    CGBuilderTy &Builder = CGF.Builder;
    bool isDerivedToBase = (E->getCastKind() == CK_DerivedToBaseMemberPointer);
    const MemberPointerType *DestTy = E->getType()->castAs<MemberPointerType>();

    if (DestTy->isMemberDataPointer()) {
      llvm::Value *Dst;
      if (isDerivedToBase)
        Dst = Builder.CreateNSWSub(Src, Adj, "adj");
      else
        Dst = Builder.CreateNSWAdd(Src, Adj, "adj");
      llvm::Value *Zero = llvm::ConstantInt::get(Src->getType(), 0);
      llvm::Value *IsNull = Builder.CreateICmpEQ(Src, Zero, "memptr.isnull");
      return Builder.CreateSelect(IsNull, Src, Dst);
    }

    llvm::Value *SrcDelta = Builder.CreateExtractValue(Src, 0, "src.delta");
    llvm::Value *TruncAdj = Builder.CreateTrunc(Adj, CGM.Int16Ty);
    llvm::Value *DstDelta;
    if (isDerivedToBase)
      DstDelta = Builder.CreateNSWSub(SrcDelta, TruncAdj, "adj");
    else
      DstDelta = Builder.CreateNSWAdd(SrcDelta, TruncAdj, "adj");
    llvm::Value *Res = Builder.CreateInsertValue(Src, DstDelta, 0);

    llvm::Value *Index = Builder.CreateExtractValue(Src, 1, "src.index");
    llvm::Value *NegOne = llvm::ConstantInt::get(Index->getType(), -1ULL, /*isSigned=*/true);
    llvm::Value *IsVirt = Builder.CreateICmpNE(Index, NegOne, "is.virt");

    llvm::BasicBlock *StartBB = Builder.GetInsertBlock();
    llvm::BasicBlock *VirtBB = CGF.createBasicBlock("ptmf.conv.virt");
    llvm::BasicBlock *EndBB = CGF.createBasicBlock("ptmf.conv.end");
    Builder.CreateCondBr(IsVirt, VirtBB, EndBB);

    CGF.EmitBlock(VirtBB);
    llvm::Value *PfnVal = Builder.CreateExtractValue(Res, 2, "src.delta2.ptr");
    llvm::Value *Delta2Val = Builder.CreateTrunc(Builder.CreatePtrToInt(PfnVal, CGM.Int32Ty), CGM.Int16Ty);
    llvm::Value *DstDelta2;
    if (isDerivedToBase)
      DstDelta2 = Builder.CreateNSWSub(Delta2Val, TruncAdj, "adj2");
    else
      DstDelta2 = Builder.CreateNSWAdd(Delta2Val, TruncAdj, "adj2");
    llvm::Value *NewPfnVal = Builder.CreateIntToPtr(Builder.CreateZExt(DstDelta2, CGM.Int32Ty), CGM.Int8PtrTy);
    llvm::Value *VirtRes = Builder.CreateInsertValue(Res, NewPfnVal, 2);
    VirtBB = Builder.GetInsertBlock();
    CGF.EmitBlock(EndBB);

    llvm::PHINode *PHI = Builder.CreatePHI(Res->getType(), 2);
    PHI->addIncoming(Res, StartBB);
    PHI->addIncoming(VirtRes, VirtBB);
    return PHI;
  }

  llvm::Constant *EmitMemberPointerConversion(const CastExpr *E, llvm::Constant *Src) override {
    assert(E->getCastKind() == CK_DerivedToBaseMemberPointer ||
           E->getCastKind() == CK_BaseToDerivedMemberPointer ||
           E->getCastKind() == CK_ReinterpretMemberPointer);

    if (E->getCastKind() == CK_ReinterpretMemberPointer)
      return Src;

    llvm::Constant *Adj = getMemberPointerAdjustment(E);
    if (!Adj)
      return Src;

    bool isDerivedToBase = (E->getCastKind() == CK_DerivedToBaseMemberPointer);
    const MemberPointerType *DestTy = E->getType()->castAs<MemberPointerType>();

    if (DestTy->isMemberDataPointer()) {
      if (Src->isNullValue())
        return Src;
      if (isDerivedToBase)
        return llvm::ConstantExpr::getNSWSub(Src, Adj);
      else
        return llvm::ConstantExpr::getNSWAdd(Src, Adj);
    }

    llvm::Constant *SrcDelta = Src->getAggregateElement(0u);
    llvm::Constant *TruncAdj = llvm::ConstantExpr::getTrunc(Adj, CGM.Int16Ty);
    llvm::Constant *DstDelta;
    if (isDerivedToBase)
      DstDelta = llvm::ConstantExpr::getNSWSub(SrcDelta, TruncAdj);
    else
      DstDelta = llvm::ConstantExpr::getNSWAdd(SrcDelta, TruncAdj);
    llvm::Constant *Res = ConstantFoldInsertValueInstruction(Src, DstDelta, 0);

    llvm::Constant *Index = Src->getAggregateElement(1u);
    if (auto *CI = dyn_cast<llvm::ConstantInt>(Index)) {
      if (!CI->isMinusOne()) {
        llvm::Constant *PfnVal = Src->getAggregateElement(2u);
        llvm::Constant *Delta2Val = llvm::ConstantExpr::getTrunc(llvm::ConstantExpr::getPtrToInt(PfnVal, CGM.Int32Ty), CGM.Int16Ty);
        llvm::Constant *DstDelta2;
        if (isDerivedToBase)
          DstDelta2 = llvm::ConstantExpr::getNSWSub(Delta2Val, TruncAdj);
        else
          DstDelta2 = llvm::ConstantExpr::getNSWAdd(Delta2Val, TruncAdj);
        llvm::Constant *NewPfnVal = llvm::ConstantExpr::getIntToPtr(llvm::ConstantExpr::getCast(llvm::Instruction::ZExt, DstDelta2, CGM.Int32Ty), CGM.Int8PtrTy);
        Res = ConstantFoldInsertValueInstruction(Res, NewPfnVal, 2);
      }
    }
    return Res;
  }

  CGCallee EmitLoadOfMemberFunctionPointer(CodeGenFunction &CGF, const Expr *E, Address ThisAddr, llvm::Value *&ThisPtrForCall, llvm::Value *MemFnPtr, const MemberPointerType *MPT) override {
    CGBuilderTy &Builder = CGF.Builder;
    const FunctionProtoType *FPT = MPT->getPointeeType()->castAs<FunctionProtoType>();
    auto *RD = MPT->getMostRecentCXXRecordDecl();

    llvm::BasicBlock *FnVirtual = CGF.createBasicBlock("memptr.virtual");
    llvm::BasicBlock *FnNonVirtual = CGF.createBasicBlock("memptr.nonvirtual");
    llvm::BasicBlock *FnEnd = CGF.createBasicBlock("memptr.end");

    llvm::Value *Delta = Builder.CreateExtractValue(MemFnPtr, 0, "memptr.delta");
    llvm::Value *Adj = Builder.CreateSExt(Delta, CGM.PtrDiffTy, "memptr.adj");

    llvm::Value *OrigThis = ThisAddr.emitRawPointer(CGF);
    llvm::Value *This = Builder.CreateInBoundsGEP(Builder.getInt8Ty(), OrigThis, Adj);
    ThisPtrForCall = This;

    llvm::Value *Index = Builder.CreateExtractValue(MemFnPtr, 1, "memptr.index");
    llvm::Value *NegOne = llvm::ConstantInt::get(Index->getType(), -1ULL, /*isSigned=*/true);
    llvm::Value *IsVirtual = Builder.CreateICmpNE(Index, NegOne, "memptr.isvirtual");
    Builder.CreateCondBr(IsVirtual, FnVirtual, FnNonVirtual);

    CGF.EmitBlock(FnVirtual);
    llvm::Value *PfnOrDelta2 = Builder.CreateExtractValue(MemFnPtr, 2, "memptr.delta2.ptr");
    llvm::Value *Delta2Val = Builder.CreateTrunc(Builder.CreatePtrToInt(PfnOrDelta2, CGM.Int32Ty), CGM.Int16Ty);
    llvm::Value *Delta2Adj = Builder.CreateSExt(Delta2Val, CGM.PtrDiffTy, "memptr.delta2.adj");

    llvm::Value *VTableThis = Builder.CreateInBoundsGEP(Builder.getInt8Ty(), OrigThis, Delta2Adj);

    llvm::Type *VTableTy = CGM.GlobalsInt8PtrTy;
    CharUnits VTablePtrAlign = CGM.getDynamicOffsetAlignment(ThisAddr.getAlignment(), RD, CGF.getPointerAlign());
    llvm::Value *VTable = CGF.GetVTablePtr(Address(VTableThis, Builder.getInt8Ty(), VTablePtrAlign), VTableTy, RD);

    llvm::Value *VTableIndex = Builder.CreateSub(Builder.CreateSExt(Index, CGM.PtrDiffTy), llvm::ConstantInt::get(CGM.PtrDiffTy, 1));
    llvm::Value *VTableOffset = Builder.CreateMul(VTableIndex, llvm::ConstantInt::get(CGM.PtrDiffTy, CGM.getPointerSize().getQuantity()));
    llvm::Value *VFPAddr = Builder.CreateGEP(CGF.Int8Ty, VTable, VTableOffset);
    llvm::Value *VirtualFn = CGF.Builder.CreateAlignedLoad(CGF.DefaultPtrTy, VFPAddr, CGF.getPointerAlign(), "memptr.virtualfn");
    CGF.EmitBranch(FnEnd);

    CGF.EmitBlock(FnNonVirtual);
    llvm::Value *NonVirtualFn = Builder.CreateExtractValue(MemFnPtr, 2, "memptr.nonvirtualfn");
    CGF.EmitBranch(FnEnd);

    CGF.EmitBlock(FnEnd);
    llvm::PHINode *CalleePtr = Builder.CreatePHI(CGF.DefaultPtrTy, 2);
    CalleePtr->addIncoming(VirtualFn, FnVirtual);
    CalleePtr->addIncoming(NonVirtualFn, FnNonVirtual);

    return CGCallee(FPT, CalleePtr, CGPointerAuthInfo());
  }


  static bool hasPolymorphicVBases(const CXXRecordDecl *RD) {
    for (const auto &B : RD->vbases()) {
      const CXXRecordDecl *VBase = B.getType()->getAsCXXRecordDecl();
      if (VBase && VBase->getNumVBases() > 0)
        return true;
    }
    return false;
  }

  llvm::Value *GetVirtualBaseClassOffset(CodeGenFunction &CGF, Address This,
                                         const CXXRecordDecl *ClassDecl,
                                         const CXXRecordDecl *BaseClassDecl) override {
    const ASTRecordLayout &Layout = CGF.getContext().getASTRecordLayout(ClassDecl);
    CharUnits Offset = Layout.getVBaseClassOffset(BaseClassDecl);
    return llvm::ConstantInt::get(CGM.PtrDiffTy, Offset.getQuantity());
  }

  void EmitCXXConstructors(const CXXConstructorDecl *D) override {
    CGM.EmitGlobal(GlobalDecl(D, Ctor_Complete));
  }
  void initializeHiddenVirtualInheritanceMembers(CodeGenFunction &CGF,
                                                 const CXXRecordDecl *RD) override {
    if (RD->getNumVBases() == 0)
      return;

    Address This = CGF.LoadCXXThisAddress();
    unsigned PtrSize = CGM.getDataLayout().getPointerSize();
    unsigned i = 0;
    for (const auto &B : RD->vbases()) {
      const CXXRecordDecl *VBaseRD = B.getType()->getAsCXXRecordDecl();
      llvm::Value *VBaseOffset = GetVirtualBaseClassOffset(CGF, This, RD, VBaseRD);
      llvm::Value *VBasePtr = CGF.Builder.CreateInBoundsGEP(CGF.Int8Ty, This.emitRawPointer(CGF), VBaseOffset);

      Address VBPtrAddr = CGF.Builder.CreateConstInBoundsByteGEP(This.withElementType(CGF.Int8Ty), CharUnits::fromQuantity(i * PtrSize));
      CGF.Builder.CreateStore(VBasePtr, VBPtrAddr.withElementType(CGM.Int8PtrTy));
      i++;
    }
  }
  bool constructorsAndDestructorsReturnThis() const override {
    return true;
  }
  AddedStructorArgCounts
  buildStructorSignature(GlobalDecl GD,
                         SmallVectorImpl<CanQualType> &ArgTys) override {
    const CXXRecordDecl *RD = nullptr;
    if (auto *MD = dyn_cast<CXXMethodDecl>(GD.getDecl()))
      RD = MD->getParent();
    if (RD && (isa<CXXDestructorDecl>(GD.getDecl()) ||
               (isa<CXXConstructorDecl>(GD.getDecl()) && RD->getNumVBases() != 0))) {
      ASTContext &Context = getContext();
      if (hasPolymorphicVBases(RD)) {
        ArgTys.insert(ArgTys.begin() + 1, Context.VoidPtrTy);
        ArgTys.insert(ArgTys.begin() + 2, Context.IntTy);
        return AddedStructorArgCounts::prefix(2);
      }
      ArgTys.insert(ArgTys.begin() + 1, Context.IntTy);
      return AddedStructorArgCounts::prefix(1);
    }
    return AddedStructorArgCounts{};
  }
  bool useThunkForDtorVariant(const CXXDestructorDecl *Dtor,
                              CXXDtorType DT) const override {
    return false;
  }
  void EmitCXXDestructors(const CXXDestructorDecl *D) override {
    CGM.EmitGlobal(GlobalDecl(D, Dtor_Base));
  }
  void addImplicitStructorParams(CodeGenFunction &CGF, QualType &ResTy,
                                 FunctionArgList &Params) override {
    const CXXMethodDecl *MD = cast<CXXMethodDecl>(CGF.CurGD.getDecl());
    const CXXRecordDecl *RD = MD->getParent();
    if (RD && (isa<CXXDestructorDecl>(MD) ||
               (isa<CXXConstructorDecl>(MD) && RD->getNumVBases() != 0))) {
      ASTContext &Context = getContext();
      auto *InChrgDecl = ImplicitParamDecl::Create(
          Context, /*DC=*/nullptr, MD->getLocation(), &Context.Idents.get("__in_chrg"),
          Context.IntTy, ImplicitParamKind::Other);

      if (hasPolymorphicVBases(RD)) {
        auto *VListDecl = ImplicitParamDecl::Create(
            Context, /*DC=*/nullptr, MD->getLocation(), &Context.Idents.get("__vlist"),
            Context.VoidPtrTy, ImplicitParamKind::Other);
        Params.insert(Params.begin() + 1, VListDecl);
        Params.insert(Params.begin() + 2, InChrgDecl);
      } else {
        Params.insert(Params.begin() + 1, InChrgDecl);
      }
      getStructorImplicitParamDecl(CGF) = InChrgDecl;
    }
  }
  void EmitInstanceFunctionProlog(CodeGenFunction &CGF) override {
    llvm::Value *ThisPtr = loadIncomingCXXThis(CGF);
    setCXXABIThisValue(CGF, ThisPtr);
    if (getStructorImplicitParamDecl(CGF)) {
      getStructorImplicitParamValue(CGF) = CGF.Builder.CreateLoad(
          CGF.GetAddrOfLocalVar(getStructorImplicitParamDecl(CGF)), "__in_chrg");
    }
    if (HasThisReturn(CGF.CurGD))
      CGF.Builder.CreateStore(getThisValue(CGF), CGF.ReturnValue);
  }
  AddedStructorArgs
  getImplicitConstructorArgs(CodeGenFunction &CGF, const CXXConstructorDecl *D,
                             CXXCtorType Type, bool ForVirtualBase,
                             bool Delegating) override {
    const CXXRecordDecl *RD = D->getParent();
    if (RD && RD->getNumVBases() != 0) {
      int InChrg = (Type == Ctor_Complete) ? 1 : 0;
      SmallVector<AddedStructorArgs::Arg, 1> Args;
      if (hasPolymorphicVBases(RD)) {
        Args.push_back({llvm::ConstantPointerNull::get(CGM.Int8PtrTy), getContext().VoidPtrTy});
      }
      Args.push_back({llvm::ConstantInt::get(CGM.Int32Ty, InChrg), getContext().IntTy});
      return AddedStructorArgs::prefix(Args);
    }
    return AddedStructorArgs{};
  }
  llvm::BasicBlock *
  EmitCtorCompleteObjectHandler(CodeGenFunction &CGF,
                                const CXXRecordDecl *RD) override {
    llvm::Value *IsMostDerivedClass = getStructorImplicitParamValue(CGF);
    assert(IsMostDerivedClass &&
           "ctor for a class with virtual bases must have an implicit parameter");
    llvm::Value *IsCompleteObject =
      CGF.Builder.CreateIsNotNull(IsMostDerivedClass, "is_complete_object");

    llvm::BasicBlock *CallVbaseCtorsBB = CGF.createBasicBlock("ctor.init_vbases");
    llvm::BasicBlock *SkipVbaseCtorsBB = CGF.createBasicBlock("ctor.skip_vbases");
    CGF.Builder.CreateCondBr(IsCompleteObject,
                             CallVbaseCtorsBB, SkipVbaseCtorsBB);

    CGF.EmitBlock(CallVbaseCtorsBB);
    return SkipVbaseCtorsBB;
  }
  llvm::Value *
  getCXXDestructorImplicitParam(CodeGenFunction &CGF,
                                const CXXDestructorDecl *DD, CXXDtorType Type,
                                bool ForVirtualBase, bool Delegating) override {
    int InChrg = 0;
    if (Type == Dtor_Deleting || Type == Dtor_VectorDeleting)
      InChrg = 3;
    else if (Type == Dtor_Complete)
      InChrg = 2;
    return llvm::ConstantInt::get(CGM.Int32Ty, InChrg);
  }
  void EmitDestructorCall(CodeGenFunction &CGF, const CXXDestructorDecl *DD,
                          CXXDtorType Type, bool ForVirtualBase,
                          bool Delegating, Address This, QualType ThisTy) override {
    GlobalDecl GD(DD, Dtor_Base);
    llvm::Constant *Fn = CGM.getAddrOfCXXStructor(GD);
    llvm::Value *InChrg = getCXXDestructorImplicitParam(CGF, DD, Type, ForVirtualBase, Delegating);

    const CXXRecordDecl *RD = DD->getParent();
    if (RD && RD->getNumVBases() != 0 && hasPolymorphicVBases(RD)) {
      llvm::Value *VList = llvm::ConstantPointerNull::get(CGM.Int8PtrTy);
      llvm::FunctionType *FTy = CGM.getTypes().GetFunctionType(CGM.getTypes().arrangeCXXStructorDeclaration(GD));
      CGF.Builder.CreateCall(FTy, Fn, {This.emitRawPointer(CGF), VList, InChrg});
    } else {
      llvm::FunctionType *FTy = CGM.getTypes().GetFunctionType(CGM.getTypes().arrangeCXXStructorDeclaration(GD));
      CGF.Builder.CreateCall(FTy, Fn, {This.emitRawPointer(CGF), InChrg});
    }
  }
  void adjustCallArgsForDestructorThunk(CodeGenFunction &CGF, GlobalDecl GD,
                                        CallArgList &CallArgs) override {
    auto *DD = cast<CXXDestructorDecl>(GD.getDecl());
    const CXXRecordDecl *RD = DD->getParent();
    auto AI = CGF.CurFn->arg_begin();
    ++AI; // skip this
    if (RD && RD->getNumVBases() != 0 && hasPolymorphicVBases(RD)) {
      llvm::Value *VList = &*AI++;
      CallArgs.add(RValue::get(VList), getContext().VoidPtrTy);
    }
    llvm::Value *InChrg = &*AI++;
    CallArgs.add(RValue::get(InChrg), getContext().IntTy);
  }
  void emitVTableDefinitions(CodeGenVTables &CGVT,
                             const CXXRecordDecl *RD) override {
    const VTableLayout &VTLayout = CGM.getItaniumVTableContext().getVTableLayout(RD);

    llvm::Type *Int8PtrTy = CGM.Int8PtrTy;
    llvm::Constant *Zero = llvm::ConstantInt::get(CGM.Int32Ty, 0);
    llvm::Constant *ZeroPtr = llvm::ConstantExpr::getIntToPtr(Zero, Int8PtrTy);

    QualType ClassType = CGM.getContext().getCanonicalTagType(RD);
    llvm::Constant *RTTI = getAddrOfRTTIFunction(ClassType);

    unsigned nextVTableThunkIndex = 0;

    for (unsigned vtableIndex = 0; vtableIndex < VTLayout.getNumVTables(); ++vtableIndex) {
      SmallString<256> Name;
      {
        llvm::raw_svector_ostream Out(Name);
        getMangleContext().mangleCXXVTable(RD, Out);
      }

      if (vtableIndex > 0) {
        const CXXRecordDecl *BaseRD = nullptr;
        for (auto &AP : VTLayout.getAddressPoints()) {
          if (AP.second.VTableIndex == vtableIndex) {
            BaseRD = AP.first.getBase();
            break;
          }
        }
        if (BaseRD) {
          SmallString<256> BaseVTName;
          {
            llvm::raw_svector_ostream Out(BaseVTName);
            getMangleContext().mangleCXXVTable(BaseRD, Out);
          }
          Name += ".";
          Name += BaseVTName.str().substr(5);
        }
      }

      llvm::GlobalVariable *VTable = CGM.getModule().getNamedGlobal(Name);
      if (!VTable) {
        llvm::Type *VTableTy = llvm::ArrayType::get(Int8PtrTy, VTLayout.getVTableSize(vtableIndex));
        VTable = new llvm::GlobalVariable(
            CGM.getModule(), VTableTy, /*isConstant=*/true,
            llvm::GlobalValue::ExternalLinkage, nullptr, Name);
      }
      if (!VTable->isDeclaration())
        continue;

      SmallVector<llvm::Constant *, 8> InitElems;
      size_t vtableStart = VTLayout.getVTableOffset(vtableIndex);
      size_t vtableEnd = vtableStart + VTLayout.getVTableSize(vtableIndex);

      for (size_t i = vtableStart; i < vtableEnd; ++i) {
        auto &comp = VTLayout.vtable_components()[i];
        switch (comp.getKind()) {
        case VTableComponent::CK_VCallOffset:
        case VTableComponent::CK_VBaseOffset:
        case VTableComponent::CK_OffsetToTop:
          if (vtableIndex > 0 && comp.getKind() == VTableComponent::CK_OffsetToTop) {
            CharUnits offset = comp.getOffsetToTop();
            llvm::Constant *offsetVal = llvm::ConstantInt::getSigned(CGM.Int32Ty, offset.getQuantity());
            InitElems.push_back(llvm::ConstantExpr::getIntToPtr(offsetVal, Int8PtrTy));
          } else {
            InitElems.push_back(ZeroPtr);
          }
          break;
        case VTableComponent::CK_RTTI:
          InitElems.push_back(llvm::ConstantExpr::getBitCast(RTTI, Int8PtrTy));
          break;
        case VTableComponent::CK_CompleteDtorPointer: {
          bool IsThunk = nextVTableThunkIndex < VTLayout.vtable_thunks().size() &&
                         VTLayout.vtable_thunks()[nextVTableThunkIndex].first == i;
          if (IsThunk)
            nextVTableThunkIndex++;
          break;
        }
        case VTableComponent::CK_DeletingDtorPointer: {
          auto *DD = cast<CXXDestructorDecl>(comp.getFunctionDecl());
          GlobalDecl GD(DD, Dtor_Base);
          bool IsThunk = nextVTableThunkIndex < VTLayout.vtable_thunks().size() &&
                         VTLayout.vtable_thunks()[nextVTableThunkIndex].first == i;
          llvm::Constant *Func;
          if (IsThunk) {
            auto &thunkInfo = VTLayout.vtable_thunks()[nextVTableThunkIndex].second;
            nextVTableThunkIndex++;
            Func = CGVT.maybeEmitThunk(GD, thunkInfo, /*ForVTable=*/true);
          } else {
            Func = CGM.getAddrOfCXXStructor(GD);
          }
          InitElems.push_back(llvm::ConstantExpr::getBitCast(Func, Int8PtrTy));
          break;
        }
        case VTableComponent::CK_FunctionPointer:
        case VTableComponent::CK_UnusedFunctionPointer: {
          GlobalDecl GD = comp.getGlobalDecl(false);
          bool IsThunk = nextVTableThunkIndex < VTLayout.vtable_thunks().size() &&
                         VTLayout.vtable_thunks()[nextVTableThunkIndex].first == i;
          llvm::Constant *Func;
          if (IsThunk) {
            auto &thunkInfo = VTLayout.vtable_thunks()[nextVTableThunkIndex].second;
            nextVTableThunkIndex++;
            Func = CGVT.maybeEmitThunk(GD, thunkInfo, /*ForVTable=*/true);
          } else {
            Func = CGM.GetAddrOfFunction(GD, Int8PtrTy);
          }
          InitElems.push_back(llvm::ConstantExpr::getBitCast(Func, Int8PtrTy));
          break;
        }
        }
      }

      llvm::Type *VTableTy = llvm::ArrayType::get(Int8PtrTy, InitElems.size());
      llvm::Constant *Init = llvm::ConstantArray::get(
          llvm::cast<llvm::ArrayType>(VTableTy), InitElems);

      llvm::GlobalVariable *OldVTable = VTable;
      StringRef OldName = OldVTable->getName();
      OldVTable->setName("");
      VTable = new llvm::GlobalVariable(
          CGM.getModule(), VTableTy, /*isConstant=*/true,
          llvm::GlobalValue::ExternalLinkage, Init, OldName);
      OldVTable->replaceAllUsesWith(
          llvm::ConstantExpr::getBitCast(VTable, OldVTable->getType()));
      OldVTable->eraseFromParent();
    }
  }

  void emitVirtualInheritanceTables(const CXXRecordDecl *RD) override {}

  bool exportThunk() override { return true; }
  void setThunkLinkage(llvm::Function *Thunk, bool ForVTable, GlobalDecl GD,
                       bool ReturnAdjustment) override {
    if (ForVTable && !Thunk->hasLocalLinkage())
      Thunk->setLinkage(llvm::GlobalValue::AvailableExternallyLinkage);
    CGM.setGVProperties(Thunk, GD);
  }

  llvm::Value *performThisAdjustment(CodeGenFunction &CGF, Address This,
                                     const CXXRecordDecl *UnadjustedClass,
                                     const ThunkInfo &TI) override {
    return performTypeAdjustment(CGF, This, UnadjustedClass, TI.This.NonVirtual,
                                 TI.This.Virtual.Itanium.VCallOffsetOffset,
                                 /*IsReturnAdjustment=*/false);
  }
  llvm::Value *performReturnAdjustment(CodeGenFunction &CGF, Address Ret,
                                       const CXXRecordDecl *UnadjustedClass,
                                       const ReturnAdjustment &RA) override {
    return performTypeAdjustment(CGF, Ret, UnadjustedClass, RA.NonVirtual,
                                 RA.Virtual.Itanium.VBaseOffsetOffset,
                                 /*IsReturnAdjustment=*/true);
  }

  size_t getSrcArgforCopyCtor(const CXXConstructorDecl *CD,
                              FunctionArgList &Args) const override {
    return 0;
  }
  StringRef GetPureVirtualCallName() override {
    return "__pure_virtual";
  }
  StringRef GetDeletedVirtualCallName() override {
    return "__pure_virtual";
  }

  void EmitGuardedInit(CodeGenFunction &CGF, const VarDecl &D,
                       llvm::GlobalVariable *var,
                       bool shouldPerformInit) override {
    CGBuilderTy &Builder = CGF.Builder;
    bool NonTemplateInline = D.isInline() && !isTemplateInstantiation(D.getTemplateSpecializationKind());
    bool threadsafe = false; // Legacy GCC 2.x ABI does not use Itanium __cxa_guard_* functions.

    if (CGF.CurFn) {
      CGF.CurFn->removeFnAttr(llvm::Attribute::AlwaysInline);
      CGF.CurFn->removeFnAttr(llvm::Attribute::InlineHint);
      CGF.CurFn->addFnAttr(llvm::Attribute::NoInline);
    }

    llvm::IntegerType *guardTy = CGF.Int32Ty;
    CharUnits guardAlignment = CharUnits::fromQuantity(CGM.getDataLayout().getABITypeAlign(guardTy));
    llvm::PointerType *guardPtrTy = llvm::PointerType::get(
        CGF.CGM.getLLVMContext(),
        CGF.CGM.getDataLayout().getDefaultGlobalsAddressSpace());

    llvm::GlobalVariable *guard = CGM.getStaticLocalDeclGuardAddress(&D);
    if (!guard) {
      SmallString<256> guardName;
      {
        llvm::raw_svector_ostream out(guardName);
        getMangleContext().mangleStaticGuardVariable(&D, out);
      }
      guard = new llvm::GlobalVariable(CGM.getModule(), guardTy,
                                       false, var->getLinkage(),
                                       llvm::ConstantInt::get(guardTy, 0),
                                       guardName.str());
      guard->setDSOLocal(var->isDSOLocal());
      guard->setVisibility(var->getVisibility());
      guard->setDLLStorageClass(var->getDLLStorageClass());
      guard->setThreadLocalMode(var->getThreadLocalMode());
      guard->setAlignment(guardAlignment.getAsAlign());

      llvm::Comdat *C = var->getComdat();
      if (!D.isLocalVarDecl() && C &&
          (CGM.getTarget().getTriple().isOSBinFormatELF() ||
           CGM.getTarget().getTriple().isOSBinFormatWasm())) {
        guard->setComdat(C);
      } else if (CGM.supportsCOMDAT() && guard->isWeakForLinker()) {
        guard->setComdat(CGM.getModule().getOrInsertComdat(guard->getName()));
      }
      CGM.setStaticLocalDeclGuardAddress(&D, guard);
    }

    Address guardAddr = Address(guard, guard->getValueType(), guardAlignment);

    llvm::BasicBlock *EndBlock = CGF.createBasicBlock("init.end");
    if (!threadsafe || CGF.getTarget().getMaxAtomicInlineWidth()) {
      llvm::LoadInst *LI = Builder.CreateLoad(guardAddr.withElementType(guardTy));
      if (threadsafe)
        LI->setAtomic(llvm::AtomicOrdering::Acquire);
      llvm::Value *NeedsInit = Builder.CreateIsNull(LI, "guard.uninitialized");
      llvm::BasicBlock *InitCheckBlock = CGF.createBasicBlock("init.check");
      CGF.EmitCXXGuardedInitBranch(NeedsInit, InitCheckBlock, EndBlock,
                                   CodeGenFunction::GuardKind::VariableGuard, &D);
      CGF.EmitBlock(InitCheckBlock);
    }

    if (threadsafe) {
      llvm::Value *V = CGF.EmitNounwindRuntimeCall(getGuardAcquireFn(CGM, guardPtrTy), guard);
      llvm::BasicBlock *InitBlock = CGF.createBasicBlock("init");
      Builder.CreateCondBr(Builder.CreateIsNotNull(V, "tobool"), InitBlock, EndBlock);
      CGF.EHStack.pushCleanup<CallGuardAbort>(EHCleanup, guard);
      CGF.EmitBlock(InitBlock);
    } else if (!D.isLocalVarDecl()) {
      Builder.CreateStore(llvm::ConstantInt::get(guardTy, 1),
                          guardAddr.withElementType(guardTy));
    }

    CGF.EmitCXXGlobalVarDeclInit(D, var, shouldPerformInit);

    if (threadsafe) {
      CGF.PopCleanupBlock();
      CGF.EmitNounwindRuntimeCall(getGuardReleaseFn(CGM, guardPtrTy),
                                  guardAddr.emitRawPointer(CGF));
    } else if (D.isLocalVarDecl()) {
      Builder.CreateStore(llvm::ConstantInt::get(guardTy, 1),
                          guardAddr.withElementType(guardTy));
    }

    CGF.EmitBlock(EndBlock);
  }
  void registerGlobalDtor(CodeGenFunction &CGF, const VarDecl &D,
                          llvm::FunctionCallee Dtor,
                          llvm::Constant *Addr) override {
    if (D.isNoDestroy(CGM.getContext()))
      return;
    if (!CGM.getLangOpts().hasAtExit() && !D.isStaticLocal())
      return CGF.registerGlobalDtorWithLLVM(D, Dtor, Addr);
    if (D.getTLSKind())
      return emitGlobalDtorWithCXAAtExit(CGF, Dtor, Addr, true);
    CGF.registerGlobalDtorWithAtExit(D, Dtor, Addr);
  }
  void EmitThreadLocalInitFuncs(
      CodeGenModule &CGM, ArrayRef<const VarDecl *> CXXThreadLocals,
      ArrayRef<llvm::Function *> CXXThreadLocalInits,
      ArrayRef<const VarDecl *> CXXThreadLocalInitVars) override {
    llvm::Function *InitFunc = nullptr;
    llvm::SmallVector<llvm::Function *, 8> OrderedInits;
    llvm::SmallDenseMap<const VarDecl *, llvm::Function *> UnorderedInits;
    for (unsigned I = 0; I != CXXThreadLocalInits.size(); ++I) {
      if (isTemplateInstantiation(
              CXXThreadLocalInitVars[I]->getTemplateSpecializationKind()))
        UnorderedInits[CXXThreadLocalInitVars[I]->getCanonicalDecl()] =
            CXXThreadLocalInits[I];
      else
        OrderedInits.push_back(CXXThreadLocalInits[I]);
    }

    if (!OrderedInits.empty()) {
      llvm::FunctionType *FTy =
          llvm::FunctionType::get(CGM.VoidTy, /*isVarArg=*/false);
      const CGFunctionInfo &FI = CGM.getTypes().arrangeNullaryFunction();
      InitFunc = CGM.CreateGlobalInitOrCleanUpFunction(FTy, "__tls_init", FI,
                                                       SourceLocation(),
                                                       /*TLS=*/true);
      llvm::GlobalVariable *Guard = new llvm::GlobalVariable(
          CGM.getModule(), CGM.Int8Ty, /*isConstant=*/false,
          llvm::GlobalVariable::InternalLinkage,
          llvm::ConstantInt::get(CGM.Int8Ty, 0), "__tls_guard");
      Guard->setThreadLocal(true);
      Guard->setThreadLocalMode(CGM.GetDefaultLLVMTLSModel());

      CharUnits GuardAlign = CharUnits::One();
      Guard->setAlignment(GuardAlign.getAsAlign());

      CodeGenFunction(CGM).GenerateCXXGlobalInitFunc(
          InitFunc, OrderedInits, ConstantAddress(Guard, CGM.Int8Ty, GuardAlign));
      if (CGM.getTarget().getTriple().isOSDarwin()) {
        InitFunc->setCallingConv(llvm::CallingConv::CXX_FAST_TLS);
        InitFunc->addFnAttr(llvm::Attribute::NoUnwind);
      }
    }

    for (const VarDecl *VD : CXXThreadLocals) {
      if (VD->hasDefinition() &&
          !isDiscardableGVALinkage(getContext().GetGVALinkageForVariable(VD))) {
        llvm::GlobalValue *GV = CGM.GetGlobalValue(CGM.getMangledName(VD));
        getOrCreateThreadLocalWrapper(VD, GV);
      }
    }

    for (auto VDAndWrapper : ThreadWrappers) {
      const VarDecl *VD = VDAndWrapper.first;
      llvm::GlobalVariable *Var =
          cast<llvm::GlobalVariable>(CGM.GetGlobalValue(CGM.getMangledName(VD)));
      llvm::Function *Wrapper = VDAndWrapper.second;

      if (!VD->hasDefinition()) {
        if (isThreadWrapperReplaceable(VD, CGM)) {
          Wrapper->setLinkage(llvm::Function::ExternalLinkage);
          continue;
        }
        if (Wrapper->getLinkage() == llvm::Function::WeakODRLinkage)
          Wrapper->setLinkage(llvm::Function::LinkOnceODRLinkage);
      }

      CGM.SetLLVMFunctionAttributesForDefinition(nullptr, Wrapper);

      SmallString<256> InitFnName;
      {
        llvm::raw_svector_ostream Out(InitFnName);
        getMangleContext().mangleItaniumThreadLocalInit(VD, Out);
      }

      llvm::FunctionType *InitFnTy = llvm::FunctionType::get(CGM.VoidTy, false);

      llvm::GlobalValue *Init = nullptr;
      bool InitIsInitFunc = false;
      bool HasConstantInitialization = false;
      if (!usesThreadWrapperFunction(VD)) {
        HasConstantInitialization = true;
      } else if (VD->hasDefinition()) {
        InitIsInitFunc = true;
        llvm::Function *InitFuncToUse = InitFunc;
        if (isTemplateInstantiation(VD->getTemplateSpecializationKind()))
          InitFuncToUse = UnorderedInits.lookup(VD->getCanonicalDecl());
        if (InitFuncToUse)
          Init = llvm::GlobalAlias::create(Var->getLinkage(), InitFnName.str(),
                                           InitFuncToUse);
      } else {
        Init = llvm::Function::Create(InitFnTy,
                                      llvm::GlobalVariable::ExternalWeakLinkage,
                                      InitFnName.str(), &CGM.getModule());
        const CGFunctionInfo &FI = CGM.getTypes().arrangeNullaryFunction();
        CGM.SetLLVMFunctionAttributes(
            GlobalDecl(), FI, cast<llvm::Function>(Init), /*IsThunk=*/false);
      }

      if (Init) {
        Init->setVisibility(Var->getVisibility());
        if (!CGM.getTriple().isOSWindows() || !Init->hasExternalWeakLinkage())
          Init->setDSOLocal(Var->isDSOLocal());
      }

      llvm::LLVMContext &Context = CGM.getModule().getContext();

      if (CGM.getTriple().isOSAIX() && VD->hasDefinition() &&
          isEmittedWithConstantInitializer(VD, true) &&
          !mayNeedDestruction(VD)) {
        assert(Init == nullptr && "Expected Init to be null.");
        llvm::Function *Func = llvm::Function::Create(
            InitFnTy, Var->getLinkage(), InitFnName.str(), &CGM.getModule());
        const CGFunctionInfo &FI = CGM.getTypes().arrangeNullaryFunction();
        CGM.SetLLVMFunctionAttributes(GlobalDecl(), FI,
                                      cast<llvm::Function>(Func),
                                      /*IsThunk=*/false);
        llvm::BasicBlock *Entry = llvm::BasicBlock::Create(Context, "", Func);
        CGBuilderTy Builder(CGM, Entry);
        Builder.CreateRetVoid();
      }

      llvm::BasicBlock *Entry = llvm::BasicBlock::Create(Context, "", Wrapper);
      CGBuilderTy Builder(CGM, Entry);
      if (HasConstantInitialization) {
      } else if (InitIsInitFunc) {
        if (Init) {
          llvm::CallInst *CallVal = Builder.CreateCall(InitFnTy, Init);
          if (isThreadWrapperReplaceable(VD, CGM)) {
            CallVal->setCallingConv(llvm::CallingConv::CXX_FAST_TLS);
            llvm::Function *Fn =
                cast<llvm::Function>(cast<llvm::GlobalAlias>(Init)->getAliasee());
            Fn->setCallingConv(llvm::CallingConv::CXX_FAST_TLS);
          }
        }
      } else if (CGM.getTriple().isOSAIX()) {
        Builder.CreateCall(InitFnTy, Init);
      } else {
        llvm::Value *Have = Builder.CreateIsNotNull(Init);
        llvm::BasicBlock *InitBB = llvm::BasicBlock::Create(Context, "", Wrapper);
        llvm::BasicBlock *ExitBB = llvm::BasicBlock::Create(Context, "", Wrapper);
        Builder.CreateCondBr(Have, InitBB, ExitBB);

        Builder.SetInsertPoint(InitBB);
        Builder.CreateCall(InitFnTy, Init);
        Builder.CreateBr(ExitBB);

        Builder.SetInsertPoint(ExitBB);
      }
      llvm::Value *Val = Builder.CreateThreadLocalAddress(Var);

      if (VD->getType()->isReferenceType()) {
        CharUnits Align = CGM.getContext().getDeclAlign(VD);
        Val = Builder.CreateAlignedLoad(Var->getValueType(), Val, Align);
      }
      Val = Builder.CreateAddrSpaceCast(Val, Wrapper->getReturnType());

      Builder.CreateRet(Val);
    }
  }
  bool usesThreadWrapperFunction(const VarDecl *VD) const override {
    return !isEmittedWithConstantInitializer(VD) || mayNeedDestruction(VD);
  }
  LValue EmitThreadLocalVarDeclLValue(CodeGenFunction &CGF, const VarDecl *VD,
                                      QualType LValType) override {
    llvm::Value *Val = CGF.CGM.GetAddrOfGlobalVar(VD);
    llvm::Function *Wrapper = getOrCreateThreadLocalWrapper(VD, Val);

    llvm::CallInst *CallVal = CGF.Builder.CreateCall(Wrapper);
    CallVal->setCallingConv(Wrapper->getCallingConv());

    LValue LV;
    if (VD->getType()->isReferenceType())
      LV = CGF.MakeNaturalAlignRawAddrLValue(CallVal, LValType);
    else
      LV = CGF.MakeRawAddrLValue(CallVal, LValType,
                                 CGF.getContext().getDeclAlign(VD));
    return LV;
  }
  void emitCXXStructor(GlobalDecl GD) override {
    if (isa<CXXConstructorDecl>(GD.getDecl()))
      GD = GD.getWithCtorType(Ctor_Complete);
    auto *MD = cast<CXXMethodDecl>(GD.getDecl());
    llvm::Function *Fn = CGM.codegenCXXStructor(GD);

    if (auto *DD = dyn_cast<CXXDestructorDecl>(MD)) {
      if (GD.getDtorType() != Dtor_Base) {
        CGM.maybeSetTrivialComdat(*MD, *Fn);
        return;
      }
      if (Fn->isDeclaration()) {
        CGM.maybeSetTrivialComdat(*MD, *Fn);
        return;
      }

      llvm::Module &M = CGM.getModule();
      llvm::LLVMContext &Ctx = M.getContext();

      std::string OrigName = Fn->getName().str();
      auto Linkage = Fn->getLinkage();
      auto Visibility = Fn->getVisibility();
      auto *Comdat = Fn->getComdat();
      auto DLLStorage = Fn->getDLLStorageClass();

      Fn->setName("__base_dtor." + OrigName);
      Fn->setLinkage(llvm::GlobalValue::InternalLinkage);
      Fn->setVisibility(llvm::GlobalValue::DefaultVisibility);
      if (Fn->hasComdat())
        Fn->setComdat(nullptr);

      llvm::Function *Wrapper = llvm::Function::Create(
          Fn->getFunctionType(), Linkage, OrigName, &M);
      Wrapper->setVisibility(Visibility);
      Wrapper->setDLLStorageClass(DLLStorage);
      if (Comdat)
        Wrapper->setComdat(Comdat);
      CGM.SetLLVMFunctionAttributes(GD, CGM.getTypes().arrangeCXXStructorDeclaration(GD), Wrapper, false);

      auto AI = Wrapper->arg_begin();
      llvm::Value *This = &*AI++;
      llvm::Value *VList = nullptr;
      if (hasPolymorphicVBases(DD->getParent())) {
        VList = &*AI++;
        VList->setName("vlist");
      }
      llvm::Value *InChrg = &*AI++;
      This->setName("this");
      InChrg->setName("in_chrg");

      llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Ctx, "entry", Wrapper);
      llvm::BasicBlock *DeleteCheckBB = llvm::BasicBlock::Create(Ctx, "dtor.delete_check", Wrapper);
      llvm::BasicBlock *DeleteBB = llvm::BasicBlock::Create(Ctx, "dtor.delete", Wrapper);
      llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(Ctx, "dtor.end", Wrapper);

      CGBuilderTy Builder(CGM, EntryBB);
      llvm::CallInst *BaseCall;
      if (VList)
        BaseCall = Builder.CreateCall(Fn, {This, VList, InChrg});
      else
        BaseCall = Builder.CreateCall(Fn, {This, InChrg});
      BaseCall->setCallingConv(Fn->getCallingConv());

      if (DD->getParent()->getNumVBases() > 0) {
        llvm::BasicBlock *VBaseDtorBB = llvm::BasicBlock::Create(Ctx, "dtor.vbases", Wrapper, DeleteCheckBB);
        llvm::Value *And2 = Builder.CreateAnd(InChrg, llvm::ConstantInt::get(CGM.Int32Ty, 2));
        llvm::Value *CondVBase = Builder.CreateIsNotNull(And2, "cond.vbases");
        Builder.CreateCondBr(CondVBase, VBaseDtorBB, DeleteCheckBB);

        CodeGenFunction CGF(CGM);
        CGF.CurGD = GD;
        CGF.CurFuncDecl = DD;
        CGF.CurFn = Wrapper;
        CGF.Builder.SetInsertPoint(VBaseDtorBB);

        for (const auto &Base : llvm::reverse(DD->getParent()->vbases())) {
          const CXXRecordDecl *VBaseRD = Base.getType()->getAsCXXRecordDecl();
          if (!VBaseRD->hasDefinition())
            continue;
          CXXDestructorDecl *VBaseDD = VBaseRD->getDestructor();
          if (!VBaseDD || VBaseDD->isTrivial())
            continue;

          GlobalDecl VBaseGD(VBaseDD, Dtor_Base);
          llvm::Constant *VBaseFn = CGM.getAddrOfCXXStructor(VBaseGD);

          Address ThisAddr(This, CGM.Int8PtrTy, CGM.getClassPointerAlignment(DD->getParent()));
          llvm::Value *VBaseOffset = GetVirtualBaseClassOffset(CGF, ThisAddr, DD->getParent(), VBaseRD);
          llvm::Value *VBaseThis = CGF.Builder.CreateInBoundsGEP(CGF.Int8Ty, This, VBaseOffset);

          llvm::FunctionType *VBaseFnTy = CGM.getTypes().GetFunctionType(CGM.getTypes().arrangeCXXStructorDeclaration(VBaseGD));
          llvm::Value *ZeroInChrg = llvm::ConstantInt::get(CGM.Int32Ty, 0);
          if (hasPolymorphicVBases(VBaseRD)) {
            llvm::Value *ZeroVList = llvm::ConstantPointerNull::get(CGM.Int8PtrTy);
            CGF.Builder.CreateCall(VBaseFnTy, VBaseFn, {VBaseThis, ZeroVList, ZeroInChrg});
          } else {
            CGF.Builder.CreateCall(VBaseFnTy, VBaseFn, {VBaseThis, ZeroInChrg});
          }
        }
        CGF.Builder.CreateBr(DeleteCheckBB);
      } else {
        Builder.CreateBr(DeleteCheckBB);
      }

      Builder.SetInsertPoint(DeleteCheckBB);
      llvm::Value *And1 = Builder.CreateAnd(InChrg, llvm::ConstantInt::get(CGM.Int32Ty, 1));
      llvm::Value *CondDelete = Builder.CreateIsNotNull(And1, "cond.delete");
      Builder.CreateCondBr(CondDelete, DeleteBB, EndBB);

      Builder.SetInsertPoint(DeleteBB);
      llvm::FunctionType *DeleteFTy = llvm::FunctionType::get(CGM.VoidTy, {CGM.Int8PtrTy}, false);
      llvm::FunctionCallee DeleteFn = CGM.CreateRuntimeFunction(DeleteFTy, "__builtin_delete");
      Builder.CreateCall(DeleteFn, {This});
      Builder.CreateBr(EndBB);

      Builder.SetInsertPoint(EndBB);
      Builder.CreateRet(This);

      CGM.maybeSetTrivialComdat(*MD, *Wrapper);
    } else {
      CGM.maybeSetTrivialComdat(*MD, *Fn);
    }
  }
  bool canSpeculativelyEmitVTable(const CXXRecordDecl *RD) const override {
    return false;
  }

  CharUnits getArrayCookieSizeImpl(QualType elementType) override {
    return std::max(CharUnits::fromQuantity(CGM.SizeSizeInBytes),
                    CGM.getContext().toCharUnitsFromBits(
                        CGM.getContext().getTargetInfo().getDoubleAlign()));
  }

  Address InitializeArrayCookie(CodeGenFunction &CGF, Address NewPtr,
                                llvm::Value *NumElements,
                                const CXXNewExpr *expr,
                                QualType ElementType) override {
    assert(requiresArrayCookie(expr));

    unsigned AS = NewPtr.getAddressSpace();
    CharUnits CookieSize = getArrayCookieSizeImpl(ElementType);

    Address NumElementsPtr = NewPtr.withElementType(CGF.SizeTy);
    llvm::Instruction *SI = CGF.Builder.CreateStore(NumElements, NumElementsPtr);

    if (CGM.getLangOpts().Sanitize.has(SanitizerKind::Address) && AS == 0 &&
        (expr->getOperatorNew()->isReplaceableGlobalAllocationFunction() ||
         CGM.getCodeGenOpts().SanitizeAddressPoisonCustomArrayCookie)) {
      SI->setNoSanitizeMetadata();
      llvm::FunctionType *FTy =
          llvm::FunctionType::get(CGM.VoidTy, NumElementsPtr.getType(), false);
      llvm::FunctionCallee F =
          CGM.CreateRuntimeFunction(FTy, "__asan_poison_cxx_array_cookie");
      CGF.Builder.CreateCall(F, NumElementsPtr.emitRawPointer(CGF));
    }

    return CGF.Builder.CreateConstInBoundsByteGEP(NewPtr, CookieSize);
  }

  llvm::Value *readArrayCookieImpl(CodeGenFunction &CGF, Address allocPtr,
                                   CharUnits cookieSize) override {
    unsigned AS = allocPtr.getAddressSpace();
    Address numElementsPtr = allocPtr.withElementType(CGF.SizeTy);
    if (!CGM.getLangOpts().Sanitize.has(SanitizerKind::Address) || AS != 0)
      return CGF.Builder.CreateLoad(numElementsPtr);

    llvm::FunctionType *FTy =
        llvm::FunctionType::get(CGF.SizeTy, CGF.DefaultPtrTy, false);
    llvm::FunctionCallee F =
        CGM.CreateRuntimeFunction(FTy, "__asan_load_cxx_array_cookie");
    return CGF.Builder.CreateCall(F, numElementsPtr.emitRawPointer(CGF));
  }
};
}

namespace clang {
namespace CodeGen {
CGCXXABI *CreateGCC2CXXABI(CodeGenModule &CGM) {
  return new GCC2CXXABI(CGM);
}
}
}
