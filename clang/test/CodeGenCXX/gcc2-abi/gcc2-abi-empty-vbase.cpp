// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Empty {};
struct Derived : virtual Empty {};

Derived d;

// CHECK: %struct.Derived = type { [8 x i8] }
// CHECK: @d ={{.*}} global %struct.Derived zeroinitializer, align 4

// CHECK: define linkonce_odr noundef ptr @__7Derivedi(ptr noundef nonnull returned align 4 dereferenceable(4) %this, i32 noundef %__in_chrg)
// CHECK: store ptr %{{.*}}, ptr %{{.*}}, align 4
