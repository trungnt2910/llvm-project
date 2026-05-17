// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -exception-model sjlj -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s --check-prefix=CHECK-SJLJ

class MyEx {};
class SubEx {};

void test_throw() {
  throw MyEx();
}
// CHECK-LABEL: define {{.*}} @test_throw__Fv(
// CHECK: call ptr @__eh_alloc(i32
// CHECK: call void @__cp_push_exception(
// CHECK: call void @__throw()

// CHECK-SJLJ-LABEL: define {{.*}} @test_throw__Fv(
// CHECK-SJLJ: call ptr @__eh_alloc(i32
// CHECK-SJLJ: call void @__cp_push_exception(
// CHECK-SJLJ: call void @__sjthrow()

void test_rethrow() {
  try {
    throw;
  } catch (...) {
  }
}
// CHECK-LABEL: define {{.*}} @test_rethrow__Fv(
// CHECK: invoke void @__throw()
// CHECK: call ptr @__start_cp_handler()
// CHECK: call void @__cp_pop_exception(

// CHECK-SJLJ-LABEL: define {{.*}} @test_rethrow__Fv(
// CHECK-SJLJ: invoke void @__sjthrow()
// CHECK-SJLJ: call ptr @__start_cp_handler()
// CHECK-SJLJ: call void @__cp_pop_exception(

void test_catch() {
  try {
    throw MyEx();
  } catch (MyEx &e) {
  } catch (SubEx &e) {
  } catch (...) {
  }
}
// CHECK-LABEL: define {{.*}} @test_catch__Fv(
// CHECK: call ptr @__start_cp_handler()
// CHECK: call void @__cp_pop_exception(

void test_terminate() throw() {
  throw MyEx();
}
// CHECK-LABEL: define {{.*}} @test_terminate__Fv(
// CHECK: call void @terminate__Fv()

void test_throw_ptr(int *p) {
  throw p;
}
// CHECK-LABEL: define {{.*}} @test_throw_ptr__FPi(
// CHECK-NOT: call ptr @__eh_alloc(
// CHECK: call void @__cp_push_exception(ptr %{{.*}}, ptr %{{.*}}, ptr null)
// CHECK: call void @__throw()

// CHECK-SJLJ-LABEL: define {{.*}} @test_throw_ptr__FPi(
// CHECK-SJLJ-NOT: call ptr @__eh_alloc(
// CHECK-SJLJ: call void @__cp_push_exception(ptr %{{.*}}, ptr %{{.*}}, ptr null)
// CHECK-SJLJ: call void @__sjthrow()

void test_catch_ptr() {
  try {
    throw (int*)0;
  } catch (const int *p) {
  }
}
// CHECK-LABEL: define {{.*}} @test_catch_ptr__Fv(
// CHECK: [[HANDLER:%.*]] = call ptr @__start_cp_handler()
// CHECK: [[VAL_PTR:%.*]] = getelementptr inbounds ptr, ptr [[HANDLER]], i64 2
// CHECK: [[EXN:%.*]] = load ptr, ptr [[VAL_PTR]], align 4
// CHECK: store ptr [[EXN]], ptr %p, align 4
