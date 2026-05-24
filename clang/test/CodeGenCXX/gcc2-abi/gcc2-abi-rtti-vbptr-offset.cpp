// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct NV {
  int nv;
};

struct V1 {
  virtual void f1();
};

// D has non-virtual base NV (size 4) and virtual base V1.
// The vbptr for V1 should be at offset 4 (after NV).
// The RTTI descriptor for D should encode offset 4 for V1.
struct D : NV, virtual V1 {
  int x;
  void f1() override;
};

void D::f1() {}

// CHECK-DAG: @__ti1D.base_list = private constant [2 x { ptr, i32 }] [{ ptr, i32 } { ptr @__ti2NV, i32 1073741824 }, { ptr, i32 } { ptr @__ti2V1, i32 1610612740 }], align 4
// CHECK-DAG: @__ti1D = weak_odr global { ptr, ptr, ptr, i32 } { ptr @.str, ptr @__vt_17__class_type_info, ptr @__ti1D.base_list, i32 2 }, align 4
