// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct PolyVBase {
  int dummy;
  PolyVBase();
  virtual ~PolyVBase();
  virtual int foo();
};

struct DerivedVList : virtual PolyVBase {
  int val;
  DerivedVList(int v);
  virtual ~DerivedVList();
  virtual int foo();
};

int test_call(DerivedVList* p) {
  return p->foo();
}

// CHECK-LABEL: define dso_local noundef i32 @test_call__FP12DerivedVList(ptr noundef %p)
// CHECK: [[THIS:%.*]] = load ptr, ptr %p.addr,
// CHECK: [[VBPTR_ADDR:%.*]] = getelementptr inbounds i8, ptr [[THIS]], i32 0
// CHECK: [[VBPTR:%.*]] = load ptr, ptr [[VBPTR_ADDR]],
// CHECK: [[VBASE_INT:%.*]] = ptrtoaddr ptr [[VBPTR]] to i32
// CHECK: [[THIS_INT:%.*]] = ptrtoaddr ptr [[THIS]] to i32
// CHECK: [[VBOFFSET:%.*]] = sub i32 [[VBASE_INT]], [[THIS_INT]]
// CHECK: [[ADJUSTED_THIS:%.*]] = getelementptr inbounds i8, ptr [[THIS]], i32 [[VBOFFSET]]
// CHECK: [[VBPTR2_ADDR:%.*]] = getelementptr inbounds i8, ptr [[THIS]], i32 0
// CHECK: [[VBPTR2:%.*]] = load ptr, ptr [[VBPTR2_ADDR]],
// CHECK: [[VBASE2_INT:%.*]] = ptrtoaddr ptr [[VBPTR2]] to i32
// CHECK: [[THIS2_INT:%.*]] = ptrtoaddr ptr [[THIS]] to i32
// CHECK: [[VBOFFSET2:%.*]] = sub i32 [[VBASE2_INT]], [[THIS2_INT]]
// CHECK: [[VBASE_PTR:%.*]] = getelementptr inbounds i8, ptr [[THIS]], i32 [[VBOFFSET2]]
// CHECK: [[VFPTR:%.*]] = getelementptr inbounds i8, ptr [[VBASE_PTR]], i32 4
// CHECK: [[VTABLE:%.*]] = load ptr, ptr [[VFPTR]],
// CHECK: [[VFN_ADDR:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 3
// CHECK: [[VFN:%.*]] = load ptr, ptr [[VFN_ADDR]],
// CHECK: call noundef i32 [[VFN]](ptr noundef nonnull align 4 dereferenceable(8) [[ADJUSTED_THIS]])
