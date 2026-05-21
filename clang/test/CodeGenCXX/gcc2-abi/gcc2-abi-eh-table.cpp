// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -std=c++98 -fexceptions -fcxx-exceptions -S %s -o - | FileCheck %s

class Derived {
public:
  Derived();
  ~Derived();
};

void trigger();

void test_eh() {
  try {
    trigger();
  } catch (Derived &e) {
  } catch (...) {
  }
}

// CHECK-LABEL: test_eh__Fv:
// CHECK: .cfi_personality 155, DW.ref.__cplus_type_matcher
// CHECK: .cfi_lsda 27, .Lexception0
// CHECK: calll trigger__Fv@PLT
//
// CHECK: .Llegacy_eh_thunk0:
// CHECK-NEXT: .byte 186 # Legacy EH Thunk for TypeID 2
// CHECK-NEXT: .long 2
// CHECK-NEXT: .byte 233
// CHECK-NEXT: .long .Ltmp2-.Ltmp4
// CHECK-NEXT: .Ltmp4:
//
// CHECK: .Llegacy_eh_thunk1:
// CHECK-NEXT: .byte 186 # Legacy EH Thunk for TypeID 1
// CHECK-NEXT: .long 1
// CHECK-NEXT: .byte 233
// CHECK-NEXT: .long .Ltmp2-.Ltmp5
// CHECK-NEXT: .Ltmp5:
//
// CHECK: .Lexception0:
// CHECK: .long -2
// CHECK: .long .Ltmp0
// CHECK-NEXT: .long .Ltmp1
// CHECK-NEXT: .long .Llegacy_eh_thunk0
// CHECK-NEXT: .long __tf7Derived
// CHECK: .long .Ltmp0
// CHECK-NEXT: .long .Ltmp1
// CHECK-NEXT: .long .Llegacy_eh_thunk1
// CHECK-NEXT: .long -1
