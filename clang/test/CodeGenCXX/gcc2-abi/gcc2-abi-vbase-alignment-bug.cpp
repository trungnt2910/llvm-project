// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct BugAlign4 { int x; };
struct BugEmpty1 {};
struct BugEmpty2 {};
struct BugVBase : BugAlign4, virtual BugEmpty1, virtual BugEmpty2 {
  BugVBase();
};

// CHECK-LABEL: define {{.*}} @__8BugVBasei(ptr noundef {{.*}}%this, i32 noundef %__in_chrg)
// CHECK: ctor.init_vbases:
// CHECK: %[[REG1:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 13
// CHECK: %[[REG2:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 4
// CHECK: store ptr %[[REG1]], ptr %[[REG2]], align 4
// CHECK: %[[REG3:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 12
// CHECK: %[[REG4:[a-zA-Z0-9_]+]] = getelementptr inbounds i8, ptr %this1, i32 8
// CHECK: store ptr %[[REG3]], ptr %[[REG4]], align 4
BugVBase::BugVBase() {}

void test_cast_empty1(BugVBase *p) {
  BugEmpty1 *b1 = p;
}
// CHECK-LABEL: define {{.*}} @test_cast_empty1__FP8BugVBase(
// CHECK: %[[VBPTR_ADDR:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 8
// CHECK: %[[VBPTR:.*]] = load ptr, ptr %[[VBPTR_ADDR]], align 4
// CHECK: %{{.*}} = sub i32 %{{.*}}, %{{.*}}

void test_cast_empty2(BugVBase *p) {
  BugEmpty2 *b2 = p;
}
// CHECK-LABEL: define {{.*}} @test_cast_empty2__FP8BugVBase(
// CHECK: %[[VBPTR_ADDR:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 4
// CHECK: %[[VBPTR:.*]] = load ptr, ptr %[[VBPTR_ADDR]], align 4
// CHECK: %{{.*}} = sub i32 %{{.*}}, %{{.*}}
