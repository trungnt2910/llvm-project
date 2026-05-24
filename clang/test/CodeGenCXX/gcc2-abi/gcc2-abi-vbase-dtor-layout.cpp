// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

class VBaseDtorTester {
public:
  int x;
  VBaseDtorTester() {}
  virtual ~VBaseDtorTester() {}
};

class VSubDtorTester : virtual public VBaseDtorTester {
public:
  int y;
  VSubDtorTester() {}
  virtual ~VSubDtorTester() {}
};

// CHECK: define {{.*}} ptr @__14VSubDtorTesteri(
// CHECK:   [[ADDPTR:%[0-9a-zA-Z_.]+]] = getelementptr inbounds i8, ptr %this1, i32 [[VBOFFSET:%[0-9a-zA-Z_.]+]]
// CHECK:   [[VFPTR:%[0-9a-zA-Z_.]+]] = getelementptr inbounds i8, ptr [[ADDPTR]], i32 4
// CHECK:   store ptr @__vt_14VSubDtorTester.15VBaseDtorTester, ptr [[VFPTR]], align 4

void test() {
  VSubDtorTester obj;
}
