// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s

class Base1 {
public:
  Base1();
  Base1(const Base1 &);
  ~Base1();
};

class Base2 {
public:
  Base2();
  Base2(const Base2 &);
  ~Base2();
};

class Derived : public Base1, public Base2 {
public:
  Derived();
  Derived(const Derived &);
  ~Derived();
};

Base1::Base1() {}
Base1::Base1(const Base1 &other) {}
Base1::~Base1() {}

Base2::Base2() {}
Base2::Base2(const Base2 &other) {}
Base2::~Base2() {}

// CHECK-LABEL: define dso_local noundef ptr @__7DerivedRC7Derived(ptr noundef nonnull returned align 1 dereferenceable(1) %this, ptr noundef nonnull align 1 dereferenceable(1) %other)
// CHECK: call noundef ptr @__5Base1RC5Base1(ptr noundef nonnull align 1 dereferenceable(1) %{{.*}}, ptr noundef nonnull align 1 dereferenceable(1) %{{.*}})
// CHECK: call noundef ptr @__5Base2RC5Base2(ptr noundef nonnull align 1 dereferenceable(1) %{{.*}}, ptr noundef nonnull align 1 dereferenceable(1) %{{.*}})
Derived::Derived(const Derived &other) : Base1(other), Base2(other) {}
Derived::~Derived() {}

void test_copy(const Derived &d) {
  Derived d2 = d;
}
// CHECK-LABEL: define dso_local void @test_copy__FRC7Derived(
// CHECK: call noundef ptr @__7DerivedRC7Derived(ptr noundef nonnull align 1 dereferenceable(1) %d2, ptr noundef nonnull align 1 dereferenceable(1) %{{.*}})
