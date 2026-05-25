// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct OrderDtor {
  int val;
  ~OrderDtor();
};

// CHECK-LABEL: define dso_local void @test_order_dtors__FG9OrderDtorN20(ptr noundef byval(%struct.OrderDtor) align 4 %a, ptr noundef byval(%struct.OrderDtor) align 4 %b, ptr noundef byval(%struct.OrderDtor) align 4 %c)
// CHECK: entry:
// CHECK: %[[DTOR_A:.*]] = call ptr @_._9OrderDtor(ptr %a, i32 2)
// CHECK-NEXT: %[[DTOR_B:.*]] = call ptr @_._9OrderDtor(ptr %b, i32 2)
// CHECK-NEXT: %[[DTOR_C:.*]] = call ptr @_._9OrderDtor(ptr %c, i32 2)
// CHECK-NEXT: ret void
void test_order_dtors(OrderDtor a, OrderDtor b, OrderDtor c) {}
