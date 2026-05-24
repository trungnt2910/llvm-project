// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

typedef int* Array[8];

// CHECK: define {{.*}} @test_arr__FPPA7_VPi
void test_arr(volatile Array** a0) {}

// CHECK: define {{.*}} @test_arr2__FPPA7_CPi
void test_arr2(const Array** a0) {}
