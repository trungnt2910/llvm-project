// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

// CHECK-LABEL: define {{.*}} @test_global_varargs__FPCce(
void test_global_varargs(const char* format, ...) {
}

class VarargsTester {
public:
  void SetToFormat(const char* format, ...);
};

// CHECK-LABEL: define {{.*}} @SetToFormat__13VarargsTesterPCce(
void VarargsTester::SetToFormat(const char* format, ...) {
}

// CHECK-LABEL: define {{.*}} @test_fn_ptr_varargs__FPFPCce_v(
void test_fn_ptr_varargs(void (*fn)(const char*, ...)) {
}
