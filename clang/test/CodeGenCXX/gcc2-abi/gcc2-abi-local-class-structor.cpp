// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Outer {
  Outer() {
    struct Local {
      void print() {}
    };
    Local obj;
    obj.print();
  }
  ~Outer() {
    struct LocalDtor {
      void print() {}
    };
    LocalDtor obj;
    obj.print();
  }
};

void test() {
  Outer obj;
}

// CHECK: define linkonce_odr void @print__Q35Outer10__5Outer.0_5Local
// CHECK: define linkonce_odr void @_I._ZZN5OuterD1EvEN9LocalDtor5printEv
