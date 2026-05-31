// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct Base {
  virtual ~Base();
};

struct Derived : public virtual Base {
  virtual ~Derived();
};

Derived g_derived;

// CHECK: define internal void @_I.__dtor_g_derived()
// CHECK: call ptr @_._7Derived(ptr @g_derived, i32 2)
