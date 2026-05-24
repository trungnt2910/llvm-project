// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

// CHECK: %struct.CDerived = type { %struct.CInterBase.base, i16, %struct.CBase }
// CHECK: %struct.CInterBase.base = type { [4 x i8], i16, i8, ptr, i16 }

struct CBase {
  virtual void print_class();
  int x;
};

struct CInterBase : public virtual CBase {
  short f_vYJS;
  bool f_Dhp;
  void* f_c8Bl;
  short f_MbUUL;
  virtual void print_class();
};

struct CDerived : public CInterBase {
  short f_hd6;
  virtual void print_class();
  CDerived();
};

// CHECK-LABEL: define dso_local noundef ptr @__8CDerivedi(
// CHECK: %[[THIS1:.*]] = load ptr, ptr %this.addr, align 4
// CHECK: %[[F_HD6:.*]] = getelementptr inbounds nuw %struct.CDerived, ptr %[[THIS1]], i32 0, i32 1
// CHECK: store i16 42, ptr %[[F_HD6]], align 4

CDerived::CDerived() {
  f_hd6 = 42;
}
