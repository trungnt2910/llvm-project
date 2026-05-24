// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

typedef int (*TdfB_Re2NfalKG)[8];

// CHECK: define dso_local void @test_qualifiers__FRPA7_iRCPA7_i(
void test_qualifiers(TdfB_Re2NfalKG& a0, const TdfB_Re2NfalKG& a1) {}
