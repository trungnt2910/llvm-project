// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -emit-llvm -o - %s | FileCheck %s

struct Base1 {
  virtual ~Base1() {}
};

struct Base2 {
  virtual ~Base2() {}
};

struct Derived : Base1, Base2 {
  virtual ~Derived() {}
};

void throw_derived() {
  throw Derived();
}

// CHECK-DAG: define linkonce_odr noundef ptr @_._7Derived(ptr{{.*}}%this, i32{{.*}}%in_chrg)
// CHECK-DAG: define internal noundef ptr @__base_dtor._._7Derived(ptr{{.*}}%this, i32{{.*}}%__in_chrg)
// CHECK-DAG: define linkonce_odr noundef ptr @__thunk_4__._7Derived(ptr{{.*}}%this, i32{{.*}}%__in_chrg)
// CHECK: call{{.*}}ptr @__base_dtor._._7Derived
