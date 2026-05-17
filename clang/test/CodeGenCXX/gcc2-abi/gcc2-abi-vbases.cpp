// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -emit-llvm %s -o - | FileCheck %s

class VBase {
public:
  VBase();
  virtual ~VBase();
};

class VSub : virtual public VBase {
public:
  VSub();
  virtual ~VSub();
};

// 1. Verify constructor signature and complete object handler
VSub::VSub() {}
// CHECK-LABEL: define {{.*}} @__4VSubi(ptr noundef {{.*}}%this, i32 noundef %__in_chrg)
// CHECK: [[IS_COMPLETE:%.*]] = icmp ne i32 %{{.*}}, 0
// CHECK: br i1 [[IS_COMPLETE]], label %ctor.init_vbases, label %ctor.skip_vbases

// 2. Verify destructor signature and wrapper logic
VSub::~VSub() {}
// CHECK-LABEL: define {{.*}} @_._4VSub(ptr noundef {{.*}}%this, i32 noundef %in_chrg)
// CHECK: call {{.*}} @__base_dtor._._4VSub(ptr %this, i32 %in_chrg)
// CHECK: [[AND2:%.*]] = and i32 %in_chrg, 2
// CHECK: [[COND_VBASES:%.*]] = icmp ne i32 [[AND2]], 0
// CHECK: br i1 [[COND_VBASES]], label %dtor.vbases, label %dtor.delete_check

// CHECK: dtor.vbases:
// CHECK: call {{.*}} @_._5VBase(ptr %{{.*}}, i32 0)
// CHECK: br label %dtor.delete_check

// CHECK: dtor.delete_check:
// CHECK: [[AND1:%.*]] = and i32 %in_chrg, 1
// CHECK: [[COND_DELETE:%.*]] = icmp ne i32 [[AND1]], 0
// CHECK: br i1 [[COND_DELETE]], label %dtor.delete, label %dtor.end

// CHECK: dtor.delete:
// CHECK: call void @__builtin_delete(ptr %this)
// CHECK: br label %dtor.end
