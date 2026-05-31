// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_C_SYMBOL -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-C
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_CPP_SYMBOL -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-CPP
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_NS_SYMBOL -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-NS
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_CLASS_INLINE -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-INLINE
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_CLASS_OUT_OF_LINE -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-OUTLINE
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_FORWARD_DECL -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-FORWARD
// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -DTEST_OPERATOR_BYPASS -emit-llvm -o - %s | FileCheck %s --check-prefix=CHECK-OPERATOR

#ifdef TEST_C_SYMBOL
// Case 1: extern "C" first symbol is not mangled.
extern "C" void first_symbol() {}
#elif defined(TEST_CPP_SYMBOL)
// Case 2: Standard C++ global first symbol is fully mangled.
void first_symbol() {}
#elif defined(TEST_NS_SYMBOL)
// Case 3: Named C++ namespace first symbol is recursively descended and mangled.
namespace N {
  void first_symbol() {}
}
#elif defined(TEST_CLASS_INLINE)
// Case 4: Inline static member function is bypassed (falls back to file hash).
struct MyClass {
  static void first_symbol() {}
};
#elif defined(TEST_CLASS_OUT_OF_LINE)
// Case 5: Out-of-class static member definition is selected and mangled with class prefix.
struct MyClass {
  static void first_symbol();
};
void MyClass::first_symbol() {}
#elif defined(TEST_FORWARD_DECL)
// Case 6: Forward declarations are bypassed (falls back to file hash).
void first_symbol();
#elif defined(TEST_OPERATOR_BYPASS)
// Case 7: Overloaded operators have no standard identifiers and must be ignored (falls back to file hash).
struct Dummy {};
bool operator!=(const Dummy&, const Dummy&) { return true; }
#endif

namespace {
  struct AnonStruct {
    void f() {}
  };
}

void use() {
  AnonStruct a;
  a.f();
}

// CHECK-C: define internal void @f__Q223_GLOBAL_.N.first_symbol10AnonStruct(
// CHECK-CPP: define internal void @f__Q227_GLOBAL_.N.first_symbol__Fv10AnonStruct(
// CHECK-NS: define internal void @f__Q228_GLOBAL_.N.first_symbol__1Nv10AnonStruct(
// CHECK-INLINE: define internal void @f__Q2{{[0-9]+}}_GLOBAL_.N.{{.*}}gcc2_abi_anonymous_namespace.cpp{{[a-zA-Z0-9]+}}10AnonStruct(
// CHECK-OUTLINE: define internal void @f__Q233_GLOBAL_.N.first_symbol__7MyClass10AnonStruct(
// CHECK-FORWARD: define internal void @f__Q2{{[0-9]+}}_GLOBAL_.N.{{.*}}gcc2_abi_anonymous_namespace.cpp{{[a-zA-Z0-9]+}}10AnonStruct(
// CHECK-OPERATOR: define internal void @f__Q2{{[0-9]+}}_GLOBAL_.N.{{.*}}gcc2_abi_anonymous_namespace.cpp{{[a-zA-Z0-9]+}}10AnonStruct(
