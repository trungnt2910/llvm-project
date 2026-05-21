// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++20 -emit-llvm %s -o - | FileCheck %s

template <double D> struct S_nontype_double {};

// CHECK: define {{.*}}test_nontype_double__FGt16S_nontype_double1d1.50000000000000000000e0(
void test_nontype_double(S_nontype_double<1.5> x) {}

// CHECK: define {{.*}}test_nontype_double_inf__FGt16S_nontype_double1dInfinity(
void test_nontype_double_inf(S_nontype_double<__builtin_inf()> x) {}

// CHECK: define {{.*}}test_nontype_double_nan__FGt16S_nontype_double1dNaN(
void test_nontype_double_nan(S_nontype_double<__builtin_nan("")> x) {}

// CHECK: define {{.*}}test_nontype_double_neginf__FGt16S_nontype_double1dmInfinity(
void test_nontype_double_neginf(S_nontype_double<-__builtin_inf()> x) {}
