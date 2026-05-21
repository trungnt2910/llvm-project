// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base1 {
  virtual void f1();
};

struct Base2 {
  virtual void f2();
};

// Derived has non-virtual dynamic Base1 and virtual Base2.
// This used to crash Clang because Base1 (primary base) is pushed to offset 4
// due to vbptr at offset 0.
struct Derived : Base1, virtual Base2 {
  virtual void f1();
  virtual void f2();
};

void Derived::f1() {}
void Derived::f2() {}

// CHECK-DAG: @__vt_7Derived = constant [3 x ptr] [ptr null, ptr @__tf7Derived, ptr @f1__7Derived]
// CHECK-DAG: @__vt_7Derived.5Base2 = constant [3 x ptr] [ptr inttoptr (i32 -8 to ptr), ptr @__tf7Derived, ptr @__thunk_8_f2__7Derived]

// CHECK: define linkonce_odr void @__thunk_8_f2__7Derived(ptr noundef %[[THIS_ARG:.*]])
// CHECK: store ptr %[[THIS_ARG]], ptr %[[ADDR:.*]],
// CHECK: %[[VAL:.*]] = load ptr, ptr %[[ADDR]],
// CHECK: %[[ADJ:.*]] = getelementptr inbounds i8, ptr %[[VAL]], i32 -8
// CHECK: tail call void @f2__7Derived(ptr noundef nonnull align 4 {{.*}} %[[ADJ]])

