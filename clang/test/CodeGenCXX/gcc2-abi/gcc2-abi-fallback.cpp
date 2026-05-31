// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++11 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s

// 1. Verify vector type (unhandled VectorType) fallback uses _I._Z... prefix
typedef int v4sf __attribute__((vector_size(16)));
void test_vector(v4sf x) {}
// CHECK: define {{.*}} @_I._Z11test_vectorDv4_i(

// 2. Verify dependent decltype (unhandled DecltypeType) fallback uses _I._Z... prefix
template<typename T, typename U>
auto test_decltype(T t, U u) -> decltype(t + u) {
  return t + u;
}
void call_decltype() {
  test_decltype(1, 2.0);
}
// CHECK: define {{.*}} @_I._Z13test_decltypeIidEDTplfp_fp0_ET_T0_(
