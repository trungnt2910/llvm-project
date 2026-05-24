// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

char g_char = 0;

namespace {
  struct AnonStruct {
    void f() {}
  };
}

void use() {
  AnonStruct a;
  a.f();
}

// CHECK: define internal void @f__Q217_GLOBAL_.N.g_char10AnonStruct(
