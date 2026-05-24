// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

// CHECK: %struct.CDerived = type { %struct.CBase, i16 }
// CHECK: %struct.CBase = type { i16, i8, ptr }

struct CBase {
  virtual void print_class();
  short f_short;
  char f_char;
};

struct CDerived : public CBase {
  short f_hd6;
  virtual void print_class();
  CDerived();
};

// CHECK-LABEL: define dso_local noundef ptr @__8CDerived(
// CHECK: %[[THIS1:.*]] = load ptr, ptr %this.addr, align 4
// CHECK: %[[F_HD6:.*]] = getelementptr inbounds nuw %struct.CDerived, ptr %[[THIS1]], i32 0, i32 1
// CHECK: store i16 42, ptr %[[F_HD6]], align 4

CDerived::CDerived() {
  f_hd6 = 42;
}
