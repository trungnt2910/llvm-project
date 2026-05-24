// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct C0 {
  virtual void print_class();
  virtual ~C0();
};
struct C1 {
  virtual void print_class();
  virtual ~C1();
};
struct C2 : public C0, public C1 {
  virtual void print_class();
  virtual ~C2();
};
struct C3 : public C0 {
  virtual void print_class();
  virtual ~C3();
};
struct C4 : public C2, public C3 {
  virtual void print_class();
  virtual ~C4();
};

void C0::print_class() {}
C0::~C0() {}
void C1::print_class() {}
C1::~C1() {}
void C2::print_class() {}
C2::~C2() {}
void C3::print_class() {}
C3::~C3() {}
void C4::print_class() {}
C4::~C4() {}

void test() {
  C4 obj;
}

// CHECK: @__vt_2C4.2C3 = constant [4 x ptr]
// CHECK-NOT: @__vt_2C4.2C0 =
