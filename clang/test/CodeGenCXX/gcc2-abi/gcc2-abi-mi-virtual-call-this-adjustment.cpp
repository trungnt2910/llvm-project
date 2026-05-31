// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -std=c++11 -emit-llvm %s -o - | FileCheck %s

// Non-dynamic base with member variables
struct NonDynamicBase {
  unsigned RefCount;
  NonDynamicBase() : RefCount(0) {}
};

// Dynamic base inheriting from non-dynamic base, adding virtual methods
struct DynamicBase : public NonDynamicBase {
  unsigned Generation;
  DynamicBase() : Generation(100) {}
  virtual ~DynamicBase() {}
  virtual int virtualMethod(int x) = 0;
};

// Abstract interfaces to push DynamicBase to a non-zero offset
struct Interface1 {
  virtual ~Interface1() {}
  virtual void method1() = 0;
};

struct Interface2 {
  virtual ~Interface2() {}
  virtual void method2() = 0;
};

struct Interface3 {
  virtual ~Interface3() {}
  virtual void method3() = 0;
};

// Final derived class inheriting from interfaces and DynamicBase
struct DerivedClass : public Interface1, public Interface2, public Interface3, public DynamicBase {
  int member;
  DerivedClass() : member(999) {}
  virtual ~DerivedClass() {}

  void method1() {}
  void method2() {}
  void method3() {}

  int virtualMethod(int x) {
    return member + x;
  }

  // CHECK-LABEL: define linkonce_odr{{.*}} noundef i32 @triggerVirtualCall__12DerivedClassi
  // CHECK: %[[THIS_ADDR:.*]] = alloca ptr
  // CHECK: %[[THIS:.*]] = load ptr, ptr %[[THIS_ADDR]]
  // CHECK: %[[ADJ:.*]] = getelementptr inbounds i8, ptr %[[THIS]], i32 12
  // CHECK: %[[ADJ2:.*]] = getelementptr inbounds i8, ptr %[[THIS]], i32 12
  // CHECK: %[[VFPTR:.*]] = getelementptr inbounds i8, ptr %[[ADJ2]], i32 8
  // CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VFPTR]]
  // CHECK: %[[SLOT:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 3
  // CHECK: %[[METHOD:.*]] = load ptr, ptr %[[SLOT]]
  // CHECK: call noundef i32 %[[METHOD]](ptr noundef{{.*}} %[[ADJ]], i32 noundef
  int triggerVirtualCall(int x) {
    return virtualMethod(x);
  }
};

DerivedClass* create_derived() {
  DerivedClass *d = new DerivedClass();
  d->triggerVirtualCall(5);
  return d;
}
