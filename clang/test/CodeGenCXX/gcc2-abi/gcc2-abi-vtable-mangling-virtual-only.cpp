// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct C1 {
  virtual void print_class();
  virtual ~C1();
};
struct C2 {
  virtual void print_class();
  virtual ~C2();
};
struct C3 : public virtual C2, public virtual C1 {
  long long f0;
  int f1;
  virtual void print_class();
  virtual ~C3();
};

void C1::print_class() {}
C1::~C1() {}
void C2::print_class() {}
C2::~C2() {}
void C3::print_class() {}
C3::~C3() {}

void test() {
  C3 obj;
}

// CHECK: @__vt_2C3.2C2 = constant [4 x ptr] [ptr inttoptr (i32 -20 to ptr), ptr @__tf2C3, ptr @__thunk_20_print_class__2C3, ptr @__thunk_20__._2C3]
// CHECK: @__vt_2C3.2C1 = constant [4 x ptr] [ptr inttoptr (i32 -24 to ptr), ptr @__tf2C3, ptr @__thunk_24_print_class__2C3, ptr @__thunk_24__._2C3]
// CHECK-NOT: @__vt_2C3 =
