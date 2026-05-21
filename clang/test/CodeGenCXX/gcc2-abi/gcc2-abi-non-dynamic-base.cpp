// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

struct NonDynamicClass {
  int a;
};

struct EmptyVBase1 {};

struct ReproClass : NonDynamicClass, virtual EmptyVBase1 {
  int x;
  ReproClass();
};

ReproClass::ReproClass() {}
// CHECK-LABEL: define {{.*}} @__10ReproClassi(ptr noundef {{.*}}%this, i32 noundef %__in_chrg)
// CHECK: ctor.init_vbases:
// CHECK: [[VBASE_ADDR:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 12
// CHECK: [[VBPTR_ADDR:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 4
// CHECK: store ptr [[VBASE_ADDR]], ptr [[VBPTR_ADDR]]
