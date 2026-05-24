// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

// CHECK: @__vt_t6Tmpl1111ZiZiZiZiZiZiZiZiZiZiZi = weak_odr constant [4 x ptr]

// 11-dimensional array
typedef int Array11[2][2][2][2][2][2][2][2][2][2][2];

// CHECK: define {{.*}} @test_arr_11__FPPA1_A1_A1_A1_A1_A1_A1_A1_A1_A1_A1_i
void test_arr_11(Array11** a0) {}

// 11 parameters function pointer typedef
typedef void (*Func11)(int, int, int, int, int, int, int, int, int, int, int);

// CHECK: define {{.*}} @test_fn_11__FPFiiiiiiiiiii_v
void test_fn_11(Func11 a0) {}

// 11 parameters template class
template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
struct Tmpl11 {
  virtual void tfunc() {}
  virtual ~Tmpl11() {}
};

// Explicit instantiation checks
template class Tmpl11<int, int, int, int, int, int, int, int, int, int, int>;
