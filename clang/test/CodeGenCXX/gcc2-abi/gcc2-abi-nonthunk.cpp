// RUN: %clang_cc1 %s -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-vtable-thunks -emit-llvm -o - | FileCheck %s

class Base1 {
public:
  Base1();
  virtual ~Base1();
  virtual void f();
};

class Base2 {
public:
  Base2();
  virtual ~Base2();
  virtual Base2* clone();
  virtual void g();
};

class Derived : public Base1, public Base2 {
public:
  Derived();
  virtual ~Derived();
  virtual void f();
  virtual Derived* clone();
  virtual void g();
};

// CHECK: @_vt.7Derived = {{.*}}constant [3 x { i16, i16, ptr }] [
// CHECK-SAME: { i16, i16, ptr } { i16 0, i16 0, ptr @__tf7Derived },
// CHECK-SAME: { i16, i16, ptr } { i16 0, i16 0, ptr @_._7Derived },
// CHECK-SAME: { i16, i16, ptr } { i16 0, i16 0, ptr @f__7Derived }
// CHECK-SAME: ]

// CHECK: @_vt.7Derived.{{.*}} = {{.*}}constant [4 x { i16, i16, ptr }] [
// CHECK-SAME: { i16, i16, ptr } { i16 -4, i16 0, ptr @__tf7Derived },
// CHECK-SAME: { i16, i16, ptr } { i16 -4, i16 0, ptr @_._7Derived },
// CHECK-SAME: { i16, i16, ptr } { i16 -4, i16 0, ptr @__thunk_0_clone__7Derived },
// CHECK-SAME: { i16, i16, ptr } { i16 -4, i16 0, ptr @g__7Derived }
// CHECK-SAME: ]

// CHECK-LABEL: define{{.*}} void @test__FP7Derived(
void test(Derived *d) {
  // CHECK: [[OBJ_PTR:%.*]] = load ptr, ptr [[D_ADDR:%.*]]
  // CHECK: [[VTABLE_PTR:%.*]] = load ptr, ptr [[OBJ_PTR]]
  // CHECK: [[ENTRY_PTR:%.*]] = getelementptr inbounds { i16, i16, ptr }, ptr [[VTABLE_PTR]], i64 2
  // CHECK: [[DELTA_PTR:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR]], i32 0, i32 0
  // CHECK: [[DELTA:%.*]] = load i16, ptr [[DELTA_PTR]]
  // CHECK: [[ADJ:%.*]] = sext i16 [[DELTA]] to i32
  // CHECK: [[ADJ_THIS:%.*]] = getelementptr inbounds i8, ptr [[OBJ_PTR]], i32 [[ADJ]]
  // CHECK: [[VTABLE_PTR2:%.*]] = load ptr, ptr [[OBJ_PTR]]
  // CHECK: [[ENTRY_PTR2:%.*]] = getelementptr inbounds { i16, i16, ptr }, ptr [[VTABLE_PTR2]], i64 2
  // CHECK: [[FUNC_PTR_PTR:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR2]], i32 0, i32 2
  // CHECK: [[FUNC_PTR:%.*]] = load ptr, ptr [[FUNC_PTR_PTR]]
  // CHECK: call void [[FUNC_PTR]](ptr noundef {{.*}}[[ADJ_THIS]])
  d->f();

  // CHECK: [[OBJ_PTR2:%.*]] = load ptr, ptr [[D_ADDR]]
  // CHECK: [[BASE2_PTR:%.*]] = getelementptr inbounds i8, ptr [[OBJ_PTR2]], i32 4
  // CHECK: [[VTABLE_PTR2:%.*]] = load ptr, ptr [[BASE2_PTR]]
  // CHECK: [[ENTRY_PTR2:%.*]] = getelementptr inbounds { i16, i16, ptr }, ptr [[VTABLE_PTR2]], i64 3
  // CHECK: [[DELTA_PTR2:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR2]], i32 0, i32 0
  // CHECK: [[DELTA2:%.*]] = load i16, ptr [[DELTA_PTR2]]
  // CHECK: [[ADJ2:%.*]] = sext i16 [[DELTA2]] to i32
  // CHECK: [[ADJ_THIS2:%.*]] = getelementptr inbounds i8, ptr [[BASE2_PTR]], i32 [[ADJ2]]
  // CHECK: [[BASE2_PTR2:%.*]] = getelementptr inbounds i8, ptr [[OBJ_PTR2]], i32 4
  // CHECK: [[VTABLE_PTR3:%.*]] = load ptr, ptr [[BASE2_PTR2]]
  // CHECK: [[ENTRY_PTR3:%.*]] = getelementptr inbounds { i16, i16, ptr }, ptr [[VTABLE_PTR3]], i64 3
  // CHECK: [[FUNC_PTR_PTR2:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR3]], i32 0, i32 2
  // CHECK: [[FUNC_PTR2:%.*]] = load ptr, ptr [[FUNC_PTR_PTR2]]
  // CHECK: call void [[FUNC_PTR2]](ptr noundef {{.*}}[[ADJ_THIS2]])
  d->g();
}

// CHECK-LABEL: define{{.*}} void @test_delete__FP7Derived(
// CHECK: [[OBJ_PTR:%.*]] = load ptr, ptr [[D_ADDR:%.*]]
// CHECK: [[ISNULL:%.*]] = icmp eq ptr [[OBJ_PTR]], null
// CHECK: br i1 [[ISNULL]], label %delete.end, label %delete.notnull
// CHECK: delete.notnull:
// CHECK: [[CAST_PTR:%.*]] = getelementptr inbounds i8, ptr [[OBJ_PTR]], i32 0
// CHECK: [[VTABLE_PTR:%.*]] = load ptr, ptr [[CAST_PTR]]
// CHECK: [[ENTRY_PTR:%.*]] = getelementptr inbounds { i16, i16, ptr }, ptr [[VTABLE_PTR]], i64 1
// CHECK: [[DELTA_PTR:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR]], i32 0, i32 0
// CHECK: [[DELTA:%.*]] = load i16, ptr [[DELTA_PTR]]
// CHECK: [[ADJ:%.*]] = sext i16 [[DELTA]] to i32
// CHECK: [[ADJ_THIS:%.*]] = getelementptr inbounds i8, ptr [[CAST_PTR]], i32 [[ADJ]]
// CHECK: [[FUNC_PTR_PTR:%.*]] = getelementptr inbounds nuw { i16, i16, ptr }, ptr [[ENTRY_PTR]], i32 0, i32 2
// CHECK: [[FUNC_PTR:%.*]] = load ptr, ptr [[FUNC_PTR_PTR]]
// CHECK: call void [[FUNC_PTR]](ptr noundef {{.*}}[[ADJ_THIS]], i32 noundef 3)

void test_delete(Derived *d) {
  delete d;
}

// CHECK-LABEL: define{{.*}} ptr @__thunk_0_clone__7Derived(
// CHECK: [[RET:%.*]] = call noundef ptr @clone__7Derived(
// CHECK: [[RET_ADJ:%.*]] = getelementptr inbounds i8, ptr [[RET]], {{i32|i64}} 4
// CHECK: [[RET_PHI:%.*]] = phi ptr [ [[RET_ADJ]], {{.*}} ], [ null, {{.*}} ]
// CHECK: ret ptr [[RET_PHI]]

Base1::Base1() {}
Base1::~Base1() {}
void Base1::f() {}

Base2::Base2() {}
Base2::~Base2() {}
Base2* Base2::clone() { return this; }
void Base2::g() {}

Derived::Derived() {}
Derived::~Derived() {}
void Derived::f() {}
Derived* Derived::clone() { return this; }
void Derived::g() {}
