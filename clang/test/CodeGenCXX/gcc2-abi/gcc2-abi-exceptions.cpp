// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s

class Derived {
public:
  Derived();
  ~Derived();
};

// 1. Verify global destructors use atexit instead of __cxa_atexit
Derived global_derived;
// CHECK: define internal void @__cxx_global_var_init()
// CHECK: call i32 @atexit(ptr @__dtor_global_derived)

// 2. Verify exception specifications call __check_eh_spec instead of __cxa_call_unexpected
void test_eh_spec() throw(Derived) {
  throw Derived();
}
// CHECK-LABEL: define {{.*}} @test_eh_spec__Fv(
// CHECK: lpad
// CHECK: filter [1 x ptr] [ptr @__ti7Derived]
// CHECK: call void @__check_eh_spec(i32 1, ptr @eh_spec_types)

// 3. Verify function-local statics use local integer guards instead of __cxa_guard_acquire / release
void test_static_local() {
  static Derived static_d;
}
// CHECK-LABEL: define {{.*}} @test_static_local__Fv(
// CHECK: [[LOAD:%.*]] = load i32, ptr @__tmp_0, align 4
// CHECK: [[TOBOOL:%.*]] = icmp eq i32 [[LOAD]], 0
// CHECK: br i1 [[TOBOOL]], label %[[INIT_CHECK:.*]], label %[[INIT_END:.*]]

// CHECK: [[INIT_CHECK]]:
// CHECK: call {{.*}} @__7Derived{{.*}}
// CHECK: call i32 @atexit(ptr @__dtor__ZZ17test_static_localvE8static_d)
// CHECK: store i32 1, ptr @__tmp_0, align 4
