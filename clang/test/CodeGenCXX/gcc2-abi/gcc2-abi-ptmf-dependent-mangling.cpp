// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm %s -o - | FileCheck %s

template <typename T, void (T::*Handler)()>
struct Helper {
  static void run(T* obj) {
    (obj->*Handler)();
  }
};

struct MyClass {
  void foo();
};

// CHECK: define {{.*}}void @run__t6Helper2Z7MyClassPM7MyClassFP7MyClass_v0_m1_13foo__7MyClassP7MyClass(ptr {{.*}})
void test() {
  MyClass obj;
  Helper<MyClass, &MyClass::foo>::run(&obj);
}
