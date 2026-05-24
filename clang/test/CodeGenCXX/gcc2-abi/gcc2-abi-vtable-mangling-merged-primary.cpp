// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct C0 {
  double f0;
  int f1;
  char f2;
  virtual void print_class();
  virtual void vfunc_0();
};

struct C1 : public C0 {
  short f0;
  virtual void print_class();
};

void C0::print_class() {}
void C0::vfunc_0() {}
void C1::print_class() {}

void test() {
  C1 obj;
}

// CHECK: @__vt_2C1 = constant [4 x ptr]
// CHECK-NOT: @__vt_2C1.2C0 =
