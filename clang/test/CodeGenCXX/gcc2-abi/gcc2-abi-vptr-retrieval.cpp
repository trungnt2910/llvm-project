// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

namespace std {
  class type_info {
  public:
    virtual ~type_info();
    const char *name() const;
  };
}

// Case 1: Virtual bases (first non-dynamic, second dynamic)
struct ReproVBaseNonDyn {
  int x;
};
struct ReproVBaseDyn {
  virtual void f();
};
struct ReproVDerived : virtual ReproVBaseNonDyn, virtual ReproVBaseDyn {
  int y;
  ReproVDerived();
};

// CHECK-LABEL: define dso_local noundef ptr @__13ReproVDerivedi(ptr noundef nonnull returned align 4 dereferenceable(12) %this, i32 noundef %__in_chrg)
// CHECK: ctor.init_vbases:
// CHECK: %[[REG1:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 16
// CHECK: %[[REG2:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 0
// CHECK: store ptr %[[REG1]], ptr %[[REG2]], align 4
// CHECK: %[[REG3:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 12
// CHECK: %[[REG4:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 4
// CHECK: store ptr %[[REG3]], ptr %[[REG4]], align 4
// CHECK: vboffset.cont:
// CHECK: %[[VFPTR:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %{{.*}}, i32 0
// CHECK: store ptr @__vt_13ReproVDerived.13ReproVBaseDyn, ptr %[[VFPTR]], align 4
ReproVDerived::ReproVDerived() {}

void test_vbase(ReproVDerived *d) {
  (void)typeid(*d);
}
// CHECK-LABEL: define dso_local void @test_vbase__FP13ReproVDerived(
// CHECK: %[[VFPTR:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 16
// CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VFPTR]], align 4
// CHECK: %[[RTTI_FN_ADDR:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 1
// CHECK: %[[RTTI_FN:.*]] = load ptr, ptr %[[RTTI_FN_ADDR]], align 4
// CHECK: call ptr %[[RTTI_FN]]()


// Case 2: Non-virtual dynamic base with members
struct ReproNVBase {
  int x;
  virtual void f();
};
struct ReproNVDerived : ReproNVBase {
  int y;
  ReproNVDerived();
};

// CHECK-LABEL: define dso_local noundef ptr @__14ReproNVDerived(ptr noundef nonnull returned align 4 dereferenceable(12) %this)
// CHECK: call noundef ptr @__11ReproNVBase(ptr noundef nonnull align 4 dereferenceable(8) %this1)
// CHECK: %[[VFPTR:.*]] = getelementptr inbounds i8, ptr %this1, i32 4
// CHECK: store ptr @__vt_14ReproNVDerived, ptr %[[VFPTR]], align 4
ReproNVDerived::ReproNVDerived() {}

void test_nvbase(ReproNVDerived *d) {
  (void)typeid(*d);
}
// CHECK-LABEL: define dso_local void @test_nvbase__FP14ReproNVDerived(
// CHECK: %[[VFPTR:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 4
// CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VFPTR]], align 4
// CHECK: %[[RTTI_FN_ADDR:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 1
// CHECK: %[[RTTI_FN:.*]] = load ptr, ptr %[[RTTI_FN_ADDR]], align 4
// CHECK: call ptr %[[RTTI_FN]]()
