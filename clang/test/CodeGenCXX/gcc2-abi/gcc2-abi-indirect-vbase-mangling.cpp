// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct CBase {
  virtual void print_class();
};
struct CSubBase : public CBase {
  virtual void print_class();
};
struct CInterBase : public virtual CSubBase {
  virtual void print_class();
};
struct CDerived : public virtual CInterBase {
  virtual void print_class();
  CDerived();
};

// Verify that CDerived constructor resolves virtual base offset dynamically using VBPtr loading
// CHECK-LABEL: define dso_local noundef ptr @__8CDerivedi(
// CHECK: vbptr.cont8:
// CHECK: %[[VBPTR_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 0
// CHECK: %[[VBPTR:.*]] = load ptr, ptr %[[VBPTR_GEP]], align 4
// CHECK: %[[VBPTR_ADDR:.*]] = ptrtoaddr ptr %[[VBPTR]] to i32
// CHECK: %[[THIS_ADDR:.*]] = ptrtoaddr ptr %this1 to i32
// CHECK: %[[VBOFFSET:.*]] = sub i32 %[[VBPTR_ADDR]], %[[THIS_ADDR]]
// CHECK: %[[VBASE_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 %[[VBOFFSET]]
// CHECK: %[[VBPTR2_GEP:.*]] = getelementptr inbounds i8, ptr %[[VBASE_GEP]], i32 0
// CHECK: %[[VBPTR2:.*]] = load ptr, ptr %[[VBPTR2_GEP]], align 4
// CHECK: %[[VBPTR2_ADDR:.*]] = ptrtoaddr ptr %[[VBPTR2]] to i32
// CHECK: %[[VBASE_ADDR:.*]] = ptrtoaddr ptr %[[VBASE_GEP]] to i32
// CHECK: %[[VBOFFSET2:.*]] = sub i32 %[[VBPTR2_ADDR]], %[[VBASE_ADDR]]
// CHECK: %[[TOTAL_OFFSET:.*]] = add i32 %[[VBOFFSET]], %{{.*}}
// CHECK: %[[ADJUSTED_THIS_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 %[[TOTAL_OFFSET]]
// CHECK: %[[VBPTR3_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 0
// CHECK: %[[VBPTR3:.*]] = load ptr, ptr %[[VBPTR3_GEP]], align 4
// CHECK: %[[VBPTR3_ADDR:.*]] = ptrtoaddr ptr %[[VBPTR3]] to i32
// CHECK: %[[THIS3_ADDR:.*]] = ptrtoaddr ptr %this1 to i32
// CHECK: %[[VBOFFSET3:.*]] = sub i32 %[[VBPTR3_ADDR]], %[[THIS3_ADDR]]
// CHECK: %[[VBASE3_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 %[[VBOFFSET3]]
// CHECK: %[[VBPTR4_GEP:.*]] = getelementptr inbounds i8, ptr %[[VBASE3_GEP]], i32 0
// CHECK: %[[VBPTR4:.*]] = load ptr, ptr %[[VBPTR4_GEP]], align 4
// CHECK: %[[VBPTR4_ADDR:.*]] = ptrtoaddr ptr %[[VBPTR4]] to i32
// CHECK: %[[VBASE3_ADDR:.*]] = ptrtoaddr ptr %[[VBASE3_GEP]] to i32
// CHECK: %[[VBOFFSET4:.*]] = sub i32 %[[VBPTR4_ADDR]], %[[VBASE3_ADDR]]
// CHECK: %[[TOTAL_OFFSET3:.*]] = add i32 %[[VBOFFSET3]], %{{.*}}
// CHECK: %[[VBASE_FINAL_GEP:.*]] = getelementptr inbounds i8, ptr %this1, i32 %[[TOTAL_OFFSET3]]
// CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VBASE_FINAL_GEP]], align 4
// CHECK: %[[RTTI_FN_ADDR:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 2
// CHECK: %[[RTTI_FN:.*]] = load ptr, ptr %[[RTTI_FN_ADDR]], align 4
// CHECK: call void %[[RTTI_FN]](ptr noundef nonnull align 4 dereferenceable(4) %[[ADJUSTED_THIS_GEP]])

CDerived::CDerived() {
  print_class();
}
