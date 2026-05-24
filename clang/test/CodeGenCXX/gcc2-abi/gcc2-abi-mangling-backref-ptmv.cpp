// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct Class {};
typedef bool Class::*PTMV;

// CHECK: define dso_local void @test_ptmv__FRPO5Class_bRVPO5Class_b(
void test_ptmv(PTMV& a0, volatile PTMV& a1) {}
