// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct DtorOnlyNonTrivial {
  int val;
  ~DtorOnlyNonTrivial();
};

DtorOnlyNonTrivial::~DtorOnlyNonTrivial() {}

// CHECK-LABEL: define dso_local void @receive_by_val__FG18DtorOnlyNonTrivial(ptr noundef byval(%struct.DtorOnlyNonTrivial) align 4 %s)
void receive_by_val(DtorOnlyNonTrivial s) {}

// CHECK-LABEL: define dso_local void @test_call__Fv()
// CHECK: %[[AGG_TMP:agg\.tmp.*]] = alloca %struct.DtorOnlyNonTrivial, align 4
// CHECK: call void @receive_by_val__FG18DtorOnlyNonTrivial(ptr noundef byval(%struct.DtorOnlyNonTrivial) align 4 %[[AGG_TMP]])
void test_call() {
  DtorOnlyNonTrivial s;
  receive_by_val(s);
}
