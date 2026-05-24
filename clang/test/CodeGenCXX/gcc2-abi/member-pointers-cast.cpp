// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct A { int a; };
struct B { int b; };
struct C : A, B { int c; };

// CHECK: @null_cast = global i32 0, align 4
int C::* null_cast = (int B::*)0;

// CHECK: @nonnull_cast = global i32 5, align 4
int C::* nonnull_cast = &B::b;

// CHECK-LABEL: define dso_local i32 @cast_dynamic__FPO1B_i(i32 %ptmd)
// CHECK: [[SRC:%.*]] = load i32, ptr %ptmd.addr
// CHECK: [[ADJ:%.*]] = add nsw i32 [[SRC]], 4
// CHECK: [[ISNULL:%.*]] = icmp eq i32 [[SRC]], 0
// CHECK: [[DST:%.*]] = select i1 [[ISNULL]], i32 [[SRC]], i32 [[ADJ]]
// CHECK: ret i32 [[DST]]
int C::* cast_dynamic(int B::* ptmd) {
  return ptmd;
}

