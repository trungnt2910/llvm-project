// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

void test_normal() {
  struct Local {
    void bar() {}
  };
  Local l;
  l.bar();
}
// CHECK: define internal void @bar__Q217test_normal__Fv.0_5Local(

template <typename T>
void test_template(T val) {
  struct Local {
    T v;
    Local(T v) : v(v) {}
    void bar() {}
  };
  Local l(val);
  l.bar();
}

template <typename T>
void test_spec(T val);

template <>
void test_spec<int>(int val) {
  struct Local {
    void bar() {}
  };
  Local l;
  l.bar();
}
// CHECK: define internal void @bar__Q223test_spec__H1Zi_X01_v.0_5Local(


void test_multiple() {
  struct Local1 {
    void bar() {}
  };
  struct Local2 {
    void bar() {}
  };
  Local1 l1; l1.bar();
  Local2 l2; l2.bar();
}
// CHECK: define internal void @bar__Q219test_multiple__Fv.0_6Local1(
// CHECK: define internal void @bar__Q219test_multiple__Fv.1_6Local2(

void test_nested() {
  struct Outer {
    struct Inner {
      void bar() {}
    };
    void bar() {}
  };
  Outer o; o.bar();
  Outer::Inner i; i.bar();
}
// CHECK: define internal void @bar__Q217test_nested__Fv.0_5Outer(
// CHECK: define internal void @bar__Q317test_nested__Fv.1_5Outer5Inner(

void instantiate() {
  test_template<int>(42);
  test_template<double>(3.14);
}
// CHECK: define linkonce_odr void @bar__Q227test_template__H1Zi_X01_v.0t5Local1Zi(
// CHECK: define linkonce_odr void @bar__Q227test_template__H1Zd_X01_v.0t5Local1Zd(
