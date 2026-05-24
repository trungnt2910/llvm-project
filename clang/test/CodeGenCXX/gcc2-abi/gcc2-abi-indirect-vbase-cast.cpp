// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct V1 {
  int v1;
  virtual void f1();
};

struct V2 : virtual V1 {
  int v2;
  virtual void f2();
};

struct D : virtual V2 {
  int d;
  virtual void fd();
};

struct Other { int o; };
struct DD : Other, virtual D {
  int dd;
};

// CHECK-LABEL: define{{.*}} ptr @test__FP1D(ptr noundef %d)
V1* test(D* d) {
  // CHECK: %[[D_VAL:.*]] = load ptr, ptr %d.addr
  // CHECK: %[[VBPTR:.*]] = getelementptr inbounds i8, ptr %[[D_VAL]], i32 0
  // CHECK: %[[VBASE2_PTR:.*]] = load ptr, ptr %[[VBPTR]]
  // CHECK: %[[VBOFFSET:.*]] = sub i32 {{.*}}, {{.*}}
  // CHECK: %[[V2:.*]] = getelementptr inbounds i8, ptr %[[D_VAL]], i32 %[[VBOFFSET]]
  // CHECK: %[[VBPTR2:.*]] = getelementptr inbounds i8, ptr %[[V2]], i32 0
  // CHECK: %[[VBASE1_PTR:.*]] = load ptr, ptr %[[VBPTR2]]
  // CHECK: %[[VBOFFSET2:.*]] = sub i32 {{.*}}, {{.*}}
  // CHECK: %[[TOTAL_OFFSET:.*]] = add i32 %[[VBOFFSET]], %[[VBOFFSET2]]
  // CHECK: %[[V1:.*]] = getelementptr inbounds i8, ptr %[[D_VAL]], i32 %[[TOTAL_OFFSET]]
  return d;
}
