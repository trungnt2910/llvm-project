// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm -o - %s | FileCheck %s

struct C {
  virtual void print_class() {}
  virtual ~C() {}
};

void test() {
  C obj;
}

// CHECK: @__vt_1C = linkonce_odr constant [4 x ptr] [ptr null, ptr null, ptr @print_class__1C, ptr @_._1C], comdat
