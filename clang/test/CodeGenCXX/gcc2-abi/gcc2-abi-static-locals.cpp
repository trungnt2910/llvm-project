// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

inline int test_inline_static(int v) {
  static int s_var = v;
  return s_var;
}

int call_it(int v) {
  return test_inline_static(v);
}

// CHECK: @s_var = internal global i32 0, align 4
// CHECK: @__tmp_0 = internal global i32 0, align 4

// CHECK: define linkonce_odr noundef i32 @test_inline_static__Fi(i32 noundef %v) #[[ATTR:[0-9]+]] comdat {
// CHECK: load i32, ptr @__tmp_0, align 4
// CHECK: store i32 1, ptr @__tmp_0, align 4

// CHECK: attributes #[[ATTR]] = { {{.*}}noinline
