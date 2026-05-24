// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct ClassSuffix0 {
  int x;
};

template <ClassSuffix0& N>
struct Tmpl {
  void f() {}
};

ClassSuffix0 g_obj_suffix0;

void use() {
  Tmpl<g_obj_suffix0> t;
  t.f();
}

// CHECK: call void @f__t4Tmpl1R12ClassSuffix0_13g_obj_suffix0(
