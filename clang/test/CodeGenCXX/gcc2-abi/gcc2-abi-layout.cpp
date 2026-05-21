// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct EboEmpty {};
// CHECK: define {{.*}} @get_size_empty
// CHECK: ret i32 1
int get_size_empty() { return sizeof(EboEmpty); }

struct LayoutBase1 { int x; };
struct __attribute__((aligned(8))) LayoutBase2 { double y; };
struct LayoutDerived1 : LayoutBase1, LayoutBase2 { char c; };

// CHECK: define {{.*}} @get_size_derived1
// CHECK: ret i32 24
int get_size_derived1() { return sizeof(LayoutDerived1); }

struct LayoutDerived2 : LayoutBase2, LayoutBase1 { char c; };
// CHECK: define {{.*}} @get_size_derived2
// CHECK: ret i32 24
int get_size_derived2() { return sizeof(LayoutDerived2); }
