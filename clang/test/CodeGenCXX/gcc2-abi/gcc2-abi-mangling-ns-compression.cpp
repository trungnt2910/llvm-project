// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

namespace N {
  struct A {
    A();
    void f(A);
    void g(A, int, A);
  };
}

// CHECK-LABEL: define {{.*}} @f__Q21N1AT0(
void N::A::f(A x) {}

// CHECK-LABEL: define {{.*}} @g__Q21N1AT0iT0(
void N::A::g(A x, int y, A z) {}

struct VeryLongClassName {
  VeryLongClassName();
  void f(VeryLongClassName);
};

// CHECK-LABEL: define {{.*}} @f__17VeryLongClassNameT0(
void VeryLongClassName::f(VeryLongClassName x) {}
