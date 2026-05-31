// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm %s -o - | FileCheck %s

template <typename T, typename Result>
Result dependent_mpt_param_helper(T* obj, Result (T::*method)()) {
  return (obj->*method)();
}

struct PTMFTestClass {
  void target_func();
};

// CHECK: define {{.*}}void @dependent_mpt_param_helper__H2Z13PTMFTestClassZv_PX01PMX01FPX01_X11_X11(ptr {{.*}}, ptr {{.*}})
void test() {
  PTMFTestClass obj;
  dependent_mpt_param_helper(&obj, &PTMFTestClass::target_func);
}
