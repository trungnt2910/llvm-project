// RUN: %clang_cc1 -std=c++98 %s -triple i386-pc-linux-gnu -fc++-abi=gcc2 -O2 -emit-llvm -o - | FileCheck %s

struct NonTrivial {
  int val;
  NonTrivial();
  NonTrivial(const NonTrivial &O);
  ~NonTrivial();
};

enum MyEnum {
  Val0 = 10,
  Val1 = 20,
  Val2 = 30
};

struct Wrapper {
  NonTrivial nt;
  MyEnum en1;
  MyEnum en2;
  MyEnum en3;
  MyEnum en4;
  MyEnum en5;
  MyEnum en6;
};

extern void use_wrapper(const Wrapper &w);

// CHECK-LABEL: define dso_local void @test_ctor_real__FRC7Wrapper(
void test_ctor_real(const Wrapper &src) {
  Wrapper local_dst = src; // Copy constructor
  use_wrapper(local_dst);
}

// CHECK: %[[DST_EN1:.*]] = getelementptr inbounds nuw i8, ptr %[[DST:.*]], i32 4
// CHECK: %[[SRC_EN1:.*]] = getelementptr inbounds nuw i8, ptr %[[SRC:.*]], i32 4
// CHECK: call void @llvm.memcpy.p0.p0.i64(ptr noundef nonnull align 4 dereferenceable(24) %[[DST_EN1]], ptr noundef nonnull align 4 dereferenceable(24) %[[SRC_EN1]], i64 24, i1 false)
