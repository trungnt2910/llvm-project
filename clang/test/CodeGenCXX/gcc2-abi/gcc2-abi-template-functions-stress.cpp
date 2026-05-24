// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

// Standalone template function
template <typename U, int N>
void tfunc_11(U a0, int a1) {}

// Explicit instantiation of standalone template function
template void tfunc_11<int, 42>(int, int);
// CHECK: define {{.*}} @tfunc_11__H2Zii42_X01i_v

struct C {
  // Member template function
  template <typename U, int N>
  void mtfunc_11(U a0, int a1) {}
};

void test() {
  C obj;
  // Member template call
  obj.mtfunc_11<int, 42>(10, 20);
}

// CHECK: define {{.*}} @mtfunc_11__H2Zii42_1CX01i_v
