// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct Class {};
typedef void (Class::*PTMF)();

struct CYKIjH4IgZGz {
  template <typename U>
  static void mt(U a0, volatile PTMF*& a1, const PTMF*& a2);
};

void test() {
  volatile PTMF* p1 = 0;
  const PTMF* p2 = 0;
  CYKIjH4IgZGz::mt<int>(10, p1, p2);
}

// CHECK: call void @mt__H1Zi_12CYKIjH4IgZGzX01RPPM5ClassFP5Class_vT2_v(
