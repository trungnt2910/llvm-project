// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct DtorOnlyNonTrivial {
  int val;
  ~DtorOnlyNonTrivial();
};

DtorOnlyNonTrivial::~DtorOnlyNonTrivial() {}

// CHECK-LABEL: define dso_local void @receive_by_val__FG18DtorOnlyNonTrivial(ptr noundef byval(%struct.DtorOnlyNonTrivial) align 4 %s)
// CHECK: entry:
// CHECK-NEXT: %0 = call ptr @_._18DtorOnlyNonTrivial(ptr %s, i32 2)
// CHECK-NEXT: ret void
void receive_by_val(DtorOnlyNonTrivial s) {}

// CHECK-LABEL: define dso_local void @test_call__Fv()
// CHECK: entry:
// CHECK: %[[S:s]] = alloca %struct.DtorOnlyNonTrivial, align 4
// CHECK: %[[AGG_TMP:agg\.tmp]] = alloca %struct.DtorOnlyNonTrivial, align 4
// CHECK: call void @llvm.memcpy.p0.p0.i32(ptr align 4 %[[AGG_TMP]], ptr align 4 %[[S]], i32 4, i1 false)
// CHECK: call void @receive_by_val__FG18DtorOnlyNonTrivial(ptr noundef byval(%struct.DtorOnlyNonTrivial) align 4 %[[AGG_TMP]])
// CHECK: %{{[0-9]+}} = call ptr @_._18DtorOnlyNonTrivial(ptr %[[S]], i32 2)
// CHECK-NOT: _._18DtorOnlyNonTrivial(ptr %[[AGG_TMP]]
// CHECK: ret void
void test_call() {
  DtorOnlyNonTrivial s;
  receive_by_val(s);
}
