// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

class Derived {
public:
  operator int();
  operator const char*();
};

// CHECK-LABEL: define {{.*}} @__opi__7Derived(
Derived::operator int() { return 42; }

// CHECK-LABEL: define {{.*}} @__opPCc__7Derived(
Derived::operator const char*() { return "hello"; }
