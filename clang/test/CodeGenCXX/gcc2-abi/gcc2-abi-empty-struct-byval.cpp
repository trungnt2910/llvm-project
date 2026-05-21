// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Empty {};
void foo(int a, Empty e, int b);

void bar() {
  Empty e;
  foo(1, e, 2);
}

// CHECK-LABEL: define dso_local void @bar__Fv(
// CHECK: call void @foo__FiG5Emptyi(i32 noundef 1, ptr noundef byval(%struct.Empty) align 4 %{{.*}}, i32 noundef 2)
