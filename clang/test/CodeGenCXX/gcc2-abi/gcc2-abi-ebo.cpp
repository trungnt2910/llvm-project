// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct EboBase1 {
  int x;
};
struct EboEmpty {
};
struct EboDerived : EboBase1, EboEmpty {
  int y;
};

// CHECK: %struct.EboDerived = type { %struct.EboBase1, [4 x i8], i32 }

EboDerived d;

int get_y(EboDerived *p) {
  return p->y;
}

// CHECK-LABEL: define {{.*}} @get_y__FP10EboDerived(
// CHECK: getelementptr inbounds{{.*}} %struct.EboDerived, ptr %{{.*}}, i32 0, i32 2
