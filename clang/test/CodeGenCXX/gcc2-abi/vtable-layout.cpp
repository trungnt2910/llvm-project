// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

class Base1 {
public:
  virtual void f1();
};

class Base2 {
public:
  virtual void f2();
};

class Derived : public Base1, public Base2 {
public:
  virtual void f1();
  virtual void f2();
};

class Base3 {
public:
  virtual void f3();
};

class ComplexDerived : public Derived, public Base3 {
public:
  virtual void f2();
  virtual void f4();
};

void Base1::f1() {}
void Base2::f2() {}
void Derived::f1() {}
void Derived::f2() {}
void Base3::f3() {}
void ComplexDerived::f2() {}
void ComplexDerived::f4() {}

// CHECK-DAG: @__vt_7Derived = constant [3 x ptr] [ptr null, ptr @__tf7Derived, ptr @f1__7Derived]
// CHECK-DAG: @__vt_14ComplexDerived = constant [4 x ptr] [ptr null, ptr @__tf14ComplexDerived, ptr @f1__7Derived, ptr @f4__14ComplexDerived]

class PolymorphicWithField {
  int field;
  virtual void f();
};
void PolymorphicWithField::f() {}
void test_poly() { PolymorphicWithField obj; }
// CHECK-DAG: %class.PolymorphicWithField = type { i32, ptr }
