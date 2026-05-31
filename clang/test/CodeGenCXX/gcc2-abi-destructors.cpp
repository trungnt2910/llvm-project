// RUN: %clang_cc1 -std=c++98 %s -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - | FileCheck %s

struct Base {
  virtual ~Base();
};

struct Derived : Base {
  virtual ~Derived();
};

Derived global_obj;

// CHECK-LABEL: define internal void @_I.__dtor_global_obj()
// CHECK: call ptr @_._7Derived(ptr @global_obj, i32 2)
// CHECK-NOT: _I._ZN7DerivedD1Ev

// CHECK-LABEL: define {{.*}}void @_GLOBAL__sub_I_
// CHECK: call void @_I.__cxx_global_var_init()
