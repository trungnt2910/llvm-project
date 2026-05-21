// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base1 {
  virtual void f1();
};

struct Base2 {
  virtual void f2();
};

struct Derived : Base1, Base2 {
  int z;
  virtual void f1();
  virtual void f2();
};

typedef void (Derived::*DerivedPTMF)();
typedef int Derived::*DerivedPTMD;

// CHECK: @ptmf_null = global { i16, i16, ptr } zeroinitializer, align 4
DerivedPTMF ptmf_null = 0;

// CHECK: @ptmd_null = global i32 0, align 4
DerivedPTMD ptmd_null = 0;

// CHECK: @ptmf_f1 = global { i16, i16, ptr } { i16 0, i16 3, ptr null }, align 4
DerivedPTMF ptmf_f1 = &Derived::f1;

// CHECK: @ptmf_f2 = global { i16, i16, ptr } { i16 4, i16 3, ptr inttoptr (i32 4 to ptr) }, align 4
DerivedPTMF ptmf_f2 = &Derived::f2;

// CHECK: @ptmf_base_to_derived = global { i16, i16, ptr } { i16 4, i16 3, ptr inttoptr (i32 4 to ptr) }, align 4
DerivedPTMF ptmf_base_to_derived = &Base2::f2;

typedef void (Base2::*Base2PTMF)();
// CHECK: @ptmf_derived_to_base = global { i16, i16, ptr } { i16 0, i16 3, ptr null }, align 4
Base2PTMF ptmf_derived_to_base = static_cast<Base2PTMF>((DerivedPTMF)&Base2::f2);

// CHECK: @ptmd_z = global i32 9, align 4
DerivedPTMD ptmd_z = &Derived::z;

// CHECK-LABEL: define dso_local void @call_ptmf__FP7DerivedPM7DerivedFP7Derived_v(
// CHECK: [[THIS:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 [[ADJ:%.*]]
// CHECK: [[IS_VIRT:%.*]] = icmp ne i16 %{{.*}}, -1
// CHECK: br i1 [[IS_VIRT]], label %memptr.virtual, label %memptr.nonvirtual

// CHECK: memptr.virtual:
// CHECK: [[VTABLE_THIS:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 [[DELTA2_ADJ:%.*]]
// CHECK: [[VTABLE:%.*]] = load ptr, ptr [[VTABLE_THIS]], align 4
// CHECK: [[VTABLE_IDX:%.*]] = sub i32 %{{.*}}, 1
// CHECK: [[VTABLE_OFFSET:%.*]] = mul i32 [[VTABLE_IDX]], 4
// CHECK: [[VFP_ADDR:%.*]] = getelementptr i8, ptr [[VTABLE]], i32 [[VTABLE_OFFSET]]
// CHECK: [[VFN:%.*]] = load ptr, ptr [[VFP_ADDR]], align 4
void call_ptmf(Derived *d, DerivedPTMF ptmf) {
  (d->*ptmf)();
}

// CHECK-LABEL: define dso_local noundef i32 @get_ptmd__FP7DerivedPO7Derived_i(
// CHECK: [[OFFSET:%.*]] = sub i32 %{{.*}}, 1
// CHECK: [[MEMPTR_OFFSET:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 [[OFFSET]]
// CHECK: [[VAL:%.*]] = load i32, ptr [[MEMPTR_OFFSET]], align 4
// CHECK: ret i32 [[VAL]]
int get_ptmd(Derived *d, DerivedPTMD ptmd) {
  return d->*ptmd;
}

typedef void (Derived::*DerivedConstPTMF)() const;

// CHECK-LABEL: define dso_local void @call_const_ptmf__FPC7DerivedPM7DerivedCFPC7Derived_v(
void call_const_ptmf(const Derived *d, DerivedConstPTMF ptmf) {
  (d->*ptmf)();
}
