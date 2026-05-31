// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

template <typename T>
struct InteropTmplDtor {
  virtual ~InteropTmplDtor() {}
};

// Instantiate it to force symbol emission
InteropTmplDtor<int> g_tmpl_obj;

// CHECK: define internal void @_I.__dtor_g_tmpl_obj()
// CHECK: call ptr @_._t15InteropTmplDtor1Zi(ptr @g_tmpl_obj, i32 2)

// CHECK: define linkonce_odr noundef ptr @_._t15InteropTmplDtor1Zi(ptr {{.*}} %this, i32 {{.*}} %in_chrg)
