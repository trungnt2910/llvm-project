// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -emit-llvm %s -o - | FileCheck %s

typedef unsigned int size_t;

class CustomAllocClass {
public:
  ~CustomAllocClass() {}

  void* operator new(size_t size);
  void operator delete(void* ptr);

  void* operator new[](size_t size);
  void operator delete[](void* ptr);
};

void test_standard_new_delete() {
  // CHECK-LABEL: define {{.*}} @test_standard_new_delete__Fv(
  // CHECK: noundef ptr @__nw__16CustomAllocClassUi(i32 noundef 1)
  CustomAllocClass *p = new CustomAllocClass();
  // CHECK: call void @__dl__16CustomAllocClassPv(ptr noundef %{{.*}})
  delete p;
}

void test_array_new_delete() {
  // CHECK-LABEL: define {{.*}} @test_array_new_delete__Fv(
  // CHECK: noundef ptr @__vn__16CustomAllocClassUi(i32 noundef 5)
  CustomAllocClass *p = new CustomAllocClass[1];
  // CHECK: call void @__vd__16CustomAllocClassPv(ptr noundef %{{.*}})
  delete[] p;
}


class CustomSizedAllocClass {
public:
  ~CustomSizedAllocClass() {}

  void operator delete(void* ptr, size_t size);
};

void test_sized_delete() {
  // CHECK-LABEL: define {{.*}} @test_sized_delete__Fv(
  CustomSizedAllocClass *p = new CustomSizedAllocClass();
  // CHECK: call void @__dl__21CustomSizedAllocClassPvUi(ptr noundef %{{.*}}, i32 noundef 1)
  delete p;
}


// Implicitly generated deleting destructors are emitted at the bottom of the IR file

// Verify that the deleting destructor compiled under GCC2 ABI calls the class-specific custom operator delete instead of __builtin_delete
// CHECK-LABEL: define {{.*}} @_._16CustomAllocClass(
// CHECK: [[AND:%.*]] = and i32 %{{.*}}, 1
// CHECK: [[COND:%.*]] = icmp ne i32 [[AND]], 0
// CHECK: br i1 [[COND]], label %[[DTOR_DELETE:.*]], label %{{.*}}
// CHECK: [[DTOR_DELETE]]:
// CHECK: call void @__dl__16CustomAllocClassPv(ptr %this)

// Verify that sized operator delete is called with the correct type size (1 byte) inside the deleting destructor
// CHECK-LABEL: define {{.*}} @_._21CustomSizedAllocClass(
// CHECK: [[AND:%.*]] = and i32 %{{.*}}, 1
// CHECK: [[COND:%.*]] = icmp ne i32 [[AND]], 0
// CHECK: br i1 [[COND]], label %[[DTOR_DELETE:.*]], label %{{.*}}
// CHECK: [[DTOR_DELETE]]:
// CHECK: call void @__dl__21CustomSizedAllocClassPvUi(ptr %this, i32 1)
