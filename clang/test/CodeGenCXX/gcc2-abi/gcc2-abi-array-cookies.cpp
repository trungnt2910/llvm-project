// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

class CookieTester {
public:
  CookieTester();
  ~CookieTester();
};

// 1. Verify new T[N] allocates N*sizeof(T) + 4 bytes, stores N at the start, and calls __builtin_vec_new
CookieTester *alloc_cookie(unsigned n) {
  return new CookieTester[n];
}
// CHECK-LABEL: define {{.*}} @alloc_cookie__FUi(
// CHECK: call { i32, i1 } @llvm.uadd.with.overflow.i32(i32 %{{.*}}, i32 4)
// CHECK: [[CALL:%.*]] = call noalias noundef nonnull ptr @__builtin_vec_new(i32 noundef %{{.*}})
// CHECK: store i32 %{{.*}}, ptr [[CALL]], align 4
// CHECK: [[GEP:%.*]] = getelementptr inbounds i8, ptr [[CALL]], i32 4
// CHECK: ret ptr [[GEP]]

// 2. Verify delete[] p accesses p - 4 to read the element count and calls __builtin_vec_delete
void delete_cookie(CookieTester *p) {
  delete[] p;
}
// CHECK-LABEL: define {{.*}} @delete_cookie__FP12CookieTester(
// CHECK: [[GEP:%.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 -4
// CHECK: [[LOAD:%.*]] = load i32, ptr [[GEP]], align 1
// CHECK: call void @__builtin_vec_delete(ptr noundef [[GEP]])
