// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm %s -o - | FileCheck %s

struct PTMFVBase {
  virtual void f();
};
struct PTMFDerived : virtual PTMFVBase {
  virtual void f();
};

template <void (PTMFDerived::*M)()> struct PTMFNontype {
  void call(PTMFDerived &obj);
};

// CHECK: declare void @call__t11PTMFNontype1PM11PTMFDerivedFP11PTMFDerived_v0_3_i0R11PTMFDerived(ptr {{.*}}, ptr {{.*}})

void test() {
  PTMFNontype<&PTMFDerived::f> obj;
  PTMFDerived d;
  obj.call(d);
}
