// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

class Base1 {
public:
  Base1();
  virtual ~Base1();
  virtual void f1();
};

class Base2 {
public:
  Base2();
  virtual ~Base2();
  virtual void f2();
};

class Derived : public Base1, public Base2 {
public:
  Derived();
  virtual ~Derived();
  virtual void f1();
  virtual void f2();
};

Base1::Base1() {}
Base1::~Base1() {}
void Base1::f1() {}

Base2::Base2() {}
Base2::~Base2() {}
void Base2::f2() {}

Derived::Derived() {}
Derived::~Derived() {}
void Derived::f1() {}
void Derived::f2() {}

void call_f2(Base2 *b2) {
  b2->f2();
}

// CHECK: @__vt_5Base1 = constant [4 x ptr] [ptr null, ptr @__tf5Base1, ptr @_._5Base1, ptr @f1__5Base1]
// CHECK: @__vt_5Base2 = constant [4 x ptr] [ptr null, ptr @__tf5Base2, ptr @_._5Base2, ptr @f2__5Base2]
// CHECK: @__vt_7Derived = constant [4 x ptr] [ptr null, ptr @__tf7Derived, ptr @_._7Derived, ptr @f1__7Derived]
// CHECK: @__vt_7Derived.5Base2 = constant [4 x ptr] [ptr inttoptr (i32 -4 to ptr), ptr @__tf7Derived, ptr @__thunk_4__._7Derived, ptr @__thunk_4_f2__7Derived]

// CHECK-LABEL: define dso_local void @call_f2__FP5Base2(
// CHECK: [[THIS:%.*]] = load ptr, ptr %{{.*}}, align 4
// CHECK: [[VTABLE:%.*]] = load ptr, ptr [[THIS]], align 4
// CHECK: [[VFN_PTR:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 3
// CHECK: [[VFN:%.*]] = load ptr, ptr [[VFN_PTR]], align 4
// CHECK: call void [[VFN]](ptr noundef nonnull align 4 dereferenceable(4) [[THIS]])
