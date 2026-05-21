// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

struct Foo {
  int z;
  virtual ~Foo();
  virtual int f1();
};

int nontype_global_var;
void nontype_global_func() {}

template <int I> struct S_nontype1 {};
template <int *P> struct S_nontype2 {};
template <void (*F)()> struct S_nontype3 {};
template <int Foo::*M> struct S_nontype4 {};
template <int (Foo::*M)()> struct S_nontype5 {};

// CHECK: define {{.*}}test_nontype_1__FGt10S_nontype11i42(
void test_nontype_1(S_nontype1<42> x) {}

// CHECK: define {{.*}}test_nontype_2__FGt10S_nontype21Pi18nontype_global_var(
void test_nontype_2(S_nontype2<&nontype_global_var> x) {}

// CHECK: define {{.*}}test_nontype_3__FGt10S_nontype31PFv_v23nontype_global_func__Fv(
void test_nontype_3(S_nontype3<&nontype_global_func> x) {}

// CHECK: define {{.*}}test_nontype_4__FGt10S_nontype41PO3Foo_i1z(
void test_nontype_4(S_nontype4<&Foo::z> x) {}

// CHECK: define {{.*}}test_nontype_5__FGt10S_nontype51PM3FooFP3Foo_i0_4_i4(
void test_nontype_5(S_nontype5<&Foo::f1> x) {}

struct Base1 {
  virtual ~Base1();
};
struct Base2 {
  virtual ~Base2();
  virtual int f2();
};
struct Derived : Base1, Base2 {
  virtual int f2();
};
template <int (Derived::*M)()> struct S_nontype6 {};

// CHECK: define {{.*}}test_nontype_7__FGt10S_nontype61PM7DerivedFP7Derived_i4_4_i4(
void test_nontype_7(S_nontype6<&Derived::f2> x) {}

struct MiBase1 {
  int x;
};
struct MiBase2 {
  virtual int foo();
};
struct MiDerived : MiBase1, MiBase2 {
  virtual int bar();
};
template <int (MiDerived::*M)()> struct S_nontype7 {};

// CHECK: define {{.*}}test_nontype_mi__FGt10S_nontype71PM9MiDerivedFP9MiDerived_i0_4_i4(
void test_nontype_mi(S_nontype7<&MiDerived::bar> x) {}
