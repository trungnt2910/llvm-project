// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base1 {
  virtual void dummy();
};
struct Base2 {
  virtual Base2* clone();
};
struct Derived : Base1, Base2 {
  virtual Derived* clone();
};

Derived* Derived::clone() { return this; }

// Verify that the thunk for Base2::clone in Derived correctly performs return-pointer adjustment (+4).
// CHECK: define weak_odr {{.*}}ptr @__thunk_4_clone__7Derived(ptr noundef %{{.*}})
// CHECK: %[[THIS:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 -4
// CHECK: %[[RET_DERIVED:.*]] = call {{.*}}ptr @clone__7Derived(ptr noundef nonnull {{.*}} %[[THIS]])
// CHECK: %[[ISNULL:.*]] = icmp eq ptr %[[RET_DERIVED]], null
// CHECK: br i1 %[[ISNULL]], label %[[ADJUST_NULL:.*]], label %[[ADJUST_NOTNULL:.*]]
// CHECK: [[ADJUST_NOTNULL]]:
// CHECK: %[[ADJUSTED:.*]] = getelementptr inbounds i8, ptr %[[RET_DERIVED]], i64 4
// CHECK: br label %[[ADJUST_END:.*]]
// CHECK: [[ADJUST_NULL]]:
// CHECK: br label %[[ADJUST_END]]
// CHECK: [[ADJUST_END]]:
// CHECK: %[[RET:.*]] = phi ptr [ %[[ADJUSTED]], %[[ADJUST_NOTNULL]] ], [ null, %[[ADJUST_NULL]] ]
// CHECK: ret ptr %[[RET]]
