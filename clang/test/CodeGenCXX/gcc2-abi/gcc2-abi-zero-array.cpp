// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

// CHECK: define{{.*}} void @test_zero_array__FPAm1_i(
void test_zero_array(int (*x)[0]) {}

// CHECK: define{{.*}} void @test_one_array__FPA0_i(
void test_one_array(int (*x)[1]) {}
