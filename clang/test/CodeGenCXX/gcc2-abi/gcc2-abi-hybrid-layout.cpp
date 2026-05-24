// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct A {
  virtual void foo();
  int a;
};
struct B : A { int b; };
struct C : virtual A { int c; };
struct D : B, C {
  int d;
  D();
};
D::D() {}

// Check that D's constructor correctly initializes the dynamic virtual base pointers
// and vtable pointers without compiler crashes.
// CHECK-LABEL: define {{.*}} @__1Di(ptr noundef {{.*}}%this, i32 noundef %__in_chrg)
