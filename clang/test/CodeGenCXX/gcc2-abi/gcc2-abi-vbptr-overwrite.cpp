// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -emit-llvm -o - %s | FileCheck %s

struct VBase {
  int v_val;
  VBase();
};

struct NonVirtualBase {
  int nv_val;
  NonVirtualBase();
};

struct Intermediate : NonVirtualBase, virtual VBase {
  int x;
  Intermediate();
};

struct Derived : Intermediate {
  int y;
  Derived();
};

// CHECK-LABEL: define dso_local noundef ptr @__12Intermediatei(ptr noundef nonnull returned align 4 dereferenceable(12) %this, i32 noundef %__in_chrg)
// CHECK-NOT:   store ptr @__vt_12Intermediate, ptr %this1, align 4
// CHECK:       ret ptr
Intermediate::Intermediate() : x(123) {}

// CHECK-LABEL: define dso_local noundef ptr @__7Derivedi(ptr noundef nonnull returned align 4 dereferenceable(16) %this, i32 noundef %__in_chrg)
// CHECK-NOT:   store ptr @__vt_7Derived, ptr %this1, align 4
// CHECK:       vbptr.init5:
// CHECK:         [[GEP1:%[0-9]+]] = getelementptr inbounds i8, ptr %this1, i32 0
// CHECK:         [[GEP2:%[0-9]+]] = getelementptr inbounds i8, ptr %this1, i32 16
// CHECK:         [[VBPTR:%[0-9]+]] = getelementptr inbounds i8, ptr [[GEP1]], i32 4
// CHECK:         store ptr [[GEP2]], ptr [[VBPTR]], align 4
Derived::Derived() : y(456) {}
