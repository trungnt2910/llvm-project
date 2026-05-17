// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

namespace std {
  class type_info {
  public:
    virtual ~type_info();
    const char *name() const;
  };
}

class MyClass {
public:
  virtual void f();
};
void MyClass::f() {}
// CHECK: @__ti7MyClass = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_16__user_type_info], align 4
// CHECK: @__tii = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_19__builtin_type_info], align 4
// CHECK: @__tiCi = weak_odr global { ptr, ptr, ptr, i32 } { ptr @{{.*}}, ptr @__vt_16__attr_type_info, ptr @__tii, i32 1 }, align 4
// CHECK: @__tiPCi = weak_odr global [3 x ptr] [ptr @{{.*}}, ptr @__vt_19__pointer_type_info, ptr @__tiCi], align 4
// CHECK: @__tiFi_v = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_16__func_type_info], align 4
// CHECK: @__tiPM7MyClassFP7MyClass_v = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_16__ptmf_type_info], align 4
// CHECK: @__tiA9_i = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_17__array_type_info], align 4

class Derived : public MyClass {
public:
  virtual void f();
};
void Derived::f() {}
// CHECK: @__ti7Derived = weak_odr global [3 x ptr] [ptr @{{.*}}, ptr @__vt_14__si_type_info, ptr @__ti7MyClass], align 4

namespace N {
  class Nested {
  public:
    virtual void g();
  };
  void Nested::g() {}
}
// CHECK: @__tiQ21N6Nested = weak_odr global [2 x ptr] [ptr @{{.*}}, ptr @__vt_16__user_type_info], align 4

const char *test_static_typeid() {
  return typeid(MyClass).name();
}
// CHECK-LABEL: define dso_local noundef ptr @test_static_typeid__Fv()
// CHECK: [[TF:%.*]] = call ptr @__tf7MyClass()
// CHECK: [[NAME:%.*]] = call noundef ptr @name__C9type_info(ptr noundef nonnull align 4 dereferenceable(4) [[TF]])
// CHECK: ret ptr [[NAME]]

// CHECK: define weak_odr ptr @__tf7MyClass()
// CHECK: ret ptr @__ti7MyClass

const char *test_dynamic_typeid(MyClass *p) {
  return typeid(*p).name();
}
// CHECK-LABEL: define dso_local noundef ptr @test_dynamic_typeid__FP7MyClass(
// CHECK: [[THIS:%.*]] = load ptr, ptr %{{.*}}, align 4
// CHECK: [[VTABLE:%.*]] = load ptr, ptr [[THIS]], align 4
// CHECK: [[RTTI_PTR:%.*]] = getelementptr inbounds ptr, ptr [[VTABLE]], i64 1
// CHECK: [[RTTI_FN:%.*]] = load ptr, ptr [[RTTI_PTR]], align 4
// CHECK: [[TI:%.*]] = call ptr [[RTTI_FN]]()
// CHECK: [[NAME:%.*]] = call noundef ptr @name__C9type_info(ptr noundef nonnull align 4 dereferenceable(4) [[TI]])
// CHECK: ret ptr [[NAME]]

const char *test_const_int_ptr_typeid() {
  return typeid(const int *).name();
}
// CHECK-LABEL: define dso_local noundef ptr @test_const_int_ptr_typeid__Fv()
// CHECK: [[TF:%.*]] = call ptr @__tfPCi()
// CHECK: [[NAME:%.*]] = call noundef ptr @name__C9type_info(ptr noundef nonnull align 4 dereferenceable(4) [[TF]])
// CHECK: ret ptr [[NAME]]

const char *test_fn_typeid() {
  return typeid(void(int)).name();
}

const char *test_ptmf_typeid() {
  return typeid(void (MyClass::*)()).name();
}

const char *test_array_typeid() {
  return typeid(int[10]).name();
}

// CHECK: define weak_odr ptr @__tfi()
// CHECK: ret ptr @__tii

// CHECK: define weak_odr ptr @__tfCi()
// CHECK: entry:
// CHECK: call ptr @__tfi()
// CHECK: ret ptr @__tiCi

// CHECK: define weak_odr ptr @__tfPCi()
// CHECK: entry:
// CHECK: call ptr @__tfCi()
// CHECK: ret ptr @__tiPCi

// CHECK: define weak_odr ptr @__tf7Derived()
// CHECK: entry:
// CHECK: call ptr @__tf7MyClass()
// CHECK: ret ptr @__ti7Derived

// CHECK: define weak_odr ptr @__tfQ21N6Nested()
// CHECK: ret ptr @__tiQ21N6Nested
