// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct alignas(8) AlignedStruct {
  double x;
};

// Target class is 8-byte aligned, members end at offset 24.
// vptr is placed at offset 24 (sizing the dynamic struct to 28).
// Padded to a multiple of 8 -> final sizeof(Target) is 32.
// The compiler must load the vptr from offset 24 (getVFPtrOffset)
// instead of wrongly using the padded size - 4 (offset 28).
struct Target {
  virtual ~Target();

  int member1;
  AlignedStruct member2;
  int member3;
  int member4;
};

void test(Target *t) {
  // CHECK: %[[VFPR:.*]] = getelementptr inbounds i8, ptr %[[T:.*]], i32 24
  // CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VFPR]]
  // CHECK: %[[SLOT2:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 2
  // CHECK: %[[FUNC:.*]] = load ptr, ptr %[[SLOT2]]
  // CHECK: call void %[[FUNC]]
  delete t;
}
