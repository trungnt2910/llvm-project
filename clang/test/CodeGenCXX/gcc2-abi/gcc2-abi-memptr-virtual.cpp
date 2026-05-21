// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct A {
  int x;
  virtual void f();
};

void (A::*p)() = &A::f;
// CHECK: @p = global { i16, i16, ptr } { i16 0, i16 3, ptr inttoptr (i32 4 to ptr) }, align 4
