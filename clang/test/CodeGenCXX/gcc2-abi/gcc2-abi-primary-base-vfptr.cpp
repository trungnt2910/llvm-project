// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct PrimaryBugC0 {
  double f0;
  int f1;
  virtual void vfunc_0();
};
struct PrimaryBugC1 {
  virtual void vfunc_1();
};
struct PrimaryBugC2 : public virtual PrimaryBugC0 {
};
struct PrimaryBugC3 : public virtual PrimaryBugC1, public PrimaryBugC2 {
  short f0;
  int f1;
  virtual void vfunc_3();
};

// CHECK: %struct.PrimaryBugC3 = type { %struct.PrimaryBugC2.base, [4 x i8], i16, i32, ptr, %struct.PrimaryBugC1, %struct.PrimaryBugC0 }

void test() {
  PrimaryBugC3 obj;
}
