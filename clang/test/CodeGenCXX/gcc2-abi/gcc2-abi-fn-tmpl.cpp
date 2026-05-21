// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

// CHECK: define {{.*}}foo__H1Zi_X01_v(
template <typename T>
void foo(T x) {}

template void foo<int>(int x);

// CHECK: define {{.*}}bar__H1Zd_iX01_v(
template <typename T>
void bar(int x, T y) {}

template void bar<double>(int x, double y);

struct S {
  // CHECK: define {{.*}}m__H1Zi_1SX01_v(
  template <typename T>
  void m(T x) {}

  // CHECK: define {{.*}}m_const__H1Zi_C1SX01_v(
  template <typename T>
  void m_const(T x) const {}
};

template void S::m<int>(int x);
template void S::m_const<int>(int x) const;
