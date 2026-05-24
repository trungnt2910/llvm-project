// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm -o - %s | FileCheck %s

struct C0 {
  char f0;
  double f1;
  virtual void print_class() {}
  virtual void vf_gUhgunHxX() {}
  virtual ~C0() {}
};
struct C1 : public C0 {
  virtual void print_class() {}
  virtual void vf_jTVkiT() {}
  virtual ~C1() {}
};
struct C2 : public virtual C0 {
  int f0;
  short f1;
  short f2;
  virtual void print_class() {}
  virtual ~C2() {}
};
struct C3 : public C2, public C1 {
  virtual void print_class() {}
  virtual ~C3() {}
};

void test() {
  C3 obj;
}

// CHECK: @__vt_2C3 = {{.*}} [6 x ptr] [ptr inttoptr (i32 -12 to ptr), ptr null, {{.*}} @__thunk_12_print_class__2C3, {{.*}} @vf_gUhgunHxX__2C0, {{.*}} @__thunk_12__._2C3, {{.*}} @vf_jTVkiT__2C1]
// CHECK: @__vt_2C3.2C0 = {{.*}} [5 x ptr] [ptr inttoptr (i32 -28 to ptr), {{.*}} @__thunk_28_print_class__2C3, {{.*}} @vf_gUhgunHxX__2C0, {{.*}} @__thunk_28__._2C3]
// CHECK-NOT: @__thunk_n12_
