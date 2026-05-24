// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -debug-info-kind=limited -emit-llvm %s -o - | FileCheck %s --check-prefix=DEBUG

// Verify compilation with -debug-info-kind=limited succeeds without crash (fixes ICE).
// DEBUG: define {{.*}} @__1Ei

struct A {
  int a;
  A();
  virtual ~A();
};

struct C : virtual A {
  int c;
  C();
  virtual ~C();
};

struct D : C {
  int d;
  D();
  virtual ~D();
};

struct E : virtual D {
  int e;
  E();
  virtual ~E();
};

E::E() : D() { e = 444; }

// CHECK-LABEL: define {{.*}} @__1Ei(ptr noundef {{.*}}%this, i32 noundef %__in_chrg)
// CHECK: %[[THIS1:.*]] = load ptr, ptr %this.addr, align 4

// CHECK: vbptr.init:
// CHECK: %[[D_ADDR:.*]] = getelementptr inbounds i8, ptr %[[THIS1]], i32 16
// CHECK: %[[E_D_VBPTR:.*]] = getelementptr inbounds i8, ptr %[[THIS1]], i32 0
// CHECK: store ptr %[[D_ADDR]], ptr %[[E_D_VBPTR]], align 4
// Verify recursive initialization of indirect virtual base pointer (C's vbptr to A).
// C's vbptr is at offset 16 (inside D), pointing to A at offset 8.
// CHECK: %[[A_ADDR:.*]] = getelementptr inbounds i8, ptr %[[THIS1]], i32 8
// CHECK: %[[C_VBPTR_ADDR:.*]] = getelementptr inbounds i8, ptr %{{[0-9]+}}, i32 0
// CHECK: store ptr %[[A_ADDR]], ptr %[[C_VBPTR_ADDR]], align 4

// CHECK: vbptr.cont:
// CHECK: %[[A_ADDR_CALL:.*]] = getelementptr inbounds i8, ptr %[[THIS1]], i32 8
// CHECK: call {{.*}}ptr @__1A(ptr noundef {{.*}}%[[A_ADDR_CALL]])
// CHECK: %[[D_ADDR_CALL:.*]] = getelementptr inbounds i8, ptr %[[THIS1]], i32 16
// CHECK: call {{.*}}ptr @__1Di(ptr noundef {{.*}}%[[D_ADDR_CALL]], i32 noundef 0)

