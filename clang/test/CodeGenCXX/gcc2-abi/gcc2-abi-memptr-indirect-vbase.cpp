// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct V {
  virtual void f() {}
  int v;
};

struct A : virtual V {
  int a;
};

struct B : A {
  virtual void f() {}
  int b;
};

void (B::*p)() = &B::f;
// CHECK: @p = global { i16, i16, ptr } { i16 12, i16 3, ptr inttoptr (i32 16 to ptr) }, align 4
