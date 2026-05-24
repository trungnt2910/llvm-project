// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

// CHECK: @std_var ={{.*}} global i32 10
// CHECK: @_7VarTest.namespace_var ={{.*}} global i32 42
// CHECK: @_Q27VarTest5Inner.nested_var ={{.*}} global i32 99
// CHECK: @global_var ={{.*}} global i32 20
// CHECK: @local_var ={{.*}} internal global i32 30

namespace Foo {

  void func() {}
  // CHECK: define{{.*}} void @func__3Foov()
  
  namespace Bar {
    void func(int x) {}
    // CHECK: define{{.*}} void @func__Q23Foo3Bari(i32{{.*}})
  }
  
  // Template function inside namespace
  template <typename T> void templ_func(T x) {}
  // CHECK: define{{.*}} void @templ_func__H1Zi_3FooX01_v(
}

template void Foo::templ_func<int>(int);

namespace A {
  namespace std {
    void nested_std_func() {}
    // CHECK: define{{.*}} void @nested_std_func__Q21A3stdv()
  }
}

// Anonymous namespace (verifies null deref fix)
namespace {
  void anon_func() {}
  // CHECK: define{{.*}} internal void @anon_func__{{[0-9]+}}_GLOBAL_.N.{{.*}}gcc2_abi_namespace.cpp{{[a-zA-Z0-9]+}}v()

  namespace std {
    void anon_std_func() {}
    // CHECK: define{{.*}} internal void @anon_std_func__Q2{{[0-9]+}}_GLOBAL_.N.{{.*}}gcc2_abi_namespace.cpp{{[a-zA-Z0-9]+}}3stdv()
  }
}

void call_anon() {
  anon_func();
  std::anon_std_func();
}

namespace ns1 {
  namespace ns2 {
    void foo() {}
    // CHECK: define{{.*}} void @foo__Q23ns13ns2v()
  }
}

namespace N1 { namespace N2 { namespace N3 { namespace N4 { namespace N5 {
namespace N6 { namespace N7 { namespace N8 { namespace N9 { namespace N10 {
  void deep_func_10() {}
  // CHECK: define{{.*}} void @deep_func_10__Q_10_2N12N22N32N42N52N62N72N82N93N10v()
}}}}}}}}}}

namespace std {
  int std_var = 10;
  void top_level_std_func() {}
  // CHECK: define{{.*}} void @top_level_std_func__Fv()
  
  namespace std {
    void nested_std_std_func() {}
    // CHECK: define{{.*}} void @nested_std_std_func__Fv()
  }
  
  namespace my_foo {
    void nested_foo_func() {}
    // CHECK: define{{.*}} void @nested_foo_func__6my_foov()
    
    namespace std {
      void nested_foo_std_func() {}
      // CHECK: define{{.*}} void @nested_foo_std_func__Q26my_foo3stdv()
    }
  }
}

namespace VarTest {
  int namespace_var = 42;
  namespace Inner {
    int nested_var = 99;
  }
}

int global_var = 20;

void local_static_test() {
  static int local_var = 30;
}

