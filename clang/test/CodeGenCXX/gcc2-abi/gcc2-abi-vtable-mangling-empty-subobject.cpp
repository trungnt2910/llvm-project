// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct C0 {
  virtual void print_class();
  virtual ~C0();
};
struct C1 : public virtual C0 {
  double f0;
  virtual void print_class();
  virtual ~C1();
};
struct C2 : public C1 {
  double f0;
  virtual void print_class();
  virtual ~C2();
};

void C0::print_class() {}
C0::~C0() {}
void C1::print_class() {}
C1::~C1() {}
void C2::print_class() {}
C2::~C2() {}

void test() {
  C2 obj;
}

// CHECK: @__vt_2C2.2C0 = constant [4 x ptr]
// CHECK-NOT: @__vt_2C2.2C1 =
// CHECK-NOT: @__vt_2C2 =
