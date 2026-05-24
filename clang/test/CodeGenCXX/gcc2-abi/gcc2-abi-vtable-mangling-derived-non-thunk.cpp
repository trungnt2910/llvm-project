// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm -o - %s | FileCheck %s

struct C0 {
  int f_xDIddE_O;
  double f_lWBnzAM0;
  double f_p0wx9OPxPbr;
  virtual void print_class() {}
  virtual ~C0() {}
};
struct C1 {
  short f_nhZWxRA;
  double f_Abhdftzb;
  virtual void print_class() {}
  virtual void vf_N55nula() {}
  virtual ~C1() {}
};
struct C2 : public virtual C0 {
  virtual void print_class() {}
  virtual ~C2() {}
};
struct C3 : public C2, public C1 {
  char f_vMQOK;
  int f_bM4Ta;
  virtual void print_class() {}
  virtual void vf_WAXCZZoOw() {}
  virtual ~C3() {}
};

void test() {
  C3 obj;
}

// CHECK: @__vt_2C3 = {{.*}} [6 x ptr] [ptr inttoptr (i32 -4 to ptr), ptr null, {{.*}} @__thunk_4_print_class__2C3, {{.*}} @vf_N55nula__2C1, {{.*}} @__thunk_4__._2C3, {{.*}} @vf_WAXCZZoOw__2C3]
// CHECK: @__vt_2C3.2C0 = {{.*}} [4 x ptr] [ptr inttoptr (i32 -28 to ptr), ptr null, {{.*}} @__thunk_28_print_class__2C3, {{.*}} @__thunk_28__._2C3]
// CHECK-NOT: @__thunk_4_vf_WAXCZZoOw
