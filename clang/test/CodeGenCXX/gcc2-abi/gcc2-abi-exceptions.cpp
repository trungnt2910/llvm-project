// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s

class Derived {
public:
  Derived();
  ~Derived();
};

// 1. Verify global destructors use atexit instead of __cxa_atexit
Derived global_derived;
// CHECK: define internal void @_I.__cxx_global_var_init()
// CHECK: call i32 @atexit(ptr @_I.__dtor_global_derived)

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
// CHECK: call i32 @atexit(ptr @_I.__dtor__ZZ17test_static_localvE8static_d)
// CHECK: store i32 1, ptr @__tmp_0, align 4

// 4. Verify const reference catching for classes uses the unqualified RTTI function in EH tables
void test_const_ref_catch() {
  try {
    throw Derived();
  } catch (const Derived &e) {
  }
}
// CHECK-LABEL: define {{.*}} @test_const_ref_catch__Fv(
// CHECK: landingpad { ptr, i32 }
// CHECK-NEXT: catch ptr @__tf7Derived
// CHECK: call i32 @llvm.eh.typeid.for.p0(ptr @__tf7Derived)

// 5. Verify nested re-throwing calls __uncatch_exception before __throw
void test_rethrow() {
  try {
    throw 42;
  } catch (int e) {
    throw;
  }
}
// CHECK-LABEL: define {{.*}} @test_rethrow__Fv(
// CHECK: landingpad { ptr, i32 }
// CHECK: invoke void @__uncatch_exception()
// CHECK: invoke void @__throw()
// CHECK: unreachable

// 6. Verify that catching a non-trivially copyable class by-value generates the copy constructor call
struct NonTrivial {
  int val;
  NonTrivial();
  NonTrivial(const NonTrivial&);
  ~NonTrivial();
};

void test_by_value_catch() {
  try {
    throw NonTrivial();
  } catch (NonTrivial e) {
  }
}
// CHECK-LABEL: define {{.*}} @test_by_value_catch__Fv(
// CHECK: landingpad { ptr, i32 }
// CHECK: [[HANDLER:%.*]] = call ptr @__start_cp_handler()
// CHECK: invoke noundef ptr @__{{.*}}NonTrivial{{.*}}(ptr {{.*}}, ptr {{.*}})
// CHECK: call ptr @_._10NonTrivial(ptr {{.*}}, i32 2)
// CHECK: call void @__cp_pop_exception(ptr [[HANDLER]])

