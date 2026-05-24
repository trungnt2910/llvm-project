// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

class MyClass {
public:
  static int stat_var;
  MyClass(int);
  ~MyClass();
  int method(char);
  virtual void vmethod();
  unsigned long long complex_method(short, bool, float *);
};

int MyClass::stat_var = 42;
// CHECK: @_7MyClass.stat_var =

int foo(int x) { return x; }
// CHECK: define {{.*}} @foo__Fi(i32

void bar(const char *p, int &r, double d) {}
// CHECK: define {{.*}} @bar__FPCcRid(

MyClass::MyClass(int x) {}
// CHECK: define {{.*}} @__7MyClassi(
// CHECK-NOT: call void @__7MyClassi(

MyClass::~MyClass() {}
// CHECK: define {{.*}} @_._7MyClass(

int MyClass::method(char c) { return c; }
// CHECK: define {{.*}} @method__7MyClassc(

void MyClass::vmethod() {}
// CHECK: define {{.*}} @vmethod__7MyClass(

unsigned long long MyClass::complex_method(short s, bool b, float *p) { return 0; }
// CHECK: define {{.*}} @complex_method__7MyClasssbPf(

namespace N {
  class Nested {
  public:
    void func();
  };
  void Nested::func() {}
}
// CHECK: define {{.*}} @func__Q21N6Nested(

class Base1 { public: virtual void f1(); };
class Base2 { public: virtual void f2(); };
class Derived : public Base1, public Base2 { public: virtual void f2(); };
void Base1::f1() {}
void Base2::f2() {}
void Derived::f2() {}


class OpTest {
public:
  bool operator==(const OpTest &) const;
  bool operator!=(const OpTest &) const;
  OpTest operator+(const OpTest &);
  OpTest &operator=(const OpTest &);
  OpTest &operator+=(const OpTest &);
  OpTest &operator-=(const OpTest &);
  OpTest &operator*=(const OpTest &);
  OpTest &operator/=(const OpTest &);
  OpTest &operator%=(const OpTest &);
  OpTest &operator&=(const OpTest &);
  OpTest &operator|=(const OpTest &);
  OpTest &operator^=(const OpTest &);
  OpTest &operator<<=(const OpTest &);
  OpTest &operator>>=(const OpTest &);
  void operator()();
  int operator[](int);
};

bool OpTest::operator==(const OpTest &) const { return true; }
// CHECK: define {{.*}} @__eq__C6OpTestRC6OpTest(

bool OpTest::operator!=(const OpTest &) const { return true; }
// CHECK: define {{.*}} @__ne__C6OpTestRC6OpTest(

OpTest OpTest::operator+(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__pl__6OpTestRC6OpTest(

OpTest &OpTest::operator=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__as__6OpTestRC6OpTest(

OpTest &OpTest::operator+=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__apl__6OpTestRC6OpTest(

OpTest &OpTest::operator-=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__ami__6OpTestRC6OpTest(

OpTest &OpTest::operator*=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__aml__6OpTestRC6OpTest(

OpTest &OpTest::operator/=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__adv__6OpTestRC6OpTest(

OpTest &OpTest::operator%=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__amd__6OpTestRC6OpTest(

OpTest &OpTest::operator&=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__aad__6OpTestRC6OpTest(

OpTest &OpTest::operator|=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__aor__6OpTestRC6OpTest(

OpTest &OpTest::operator^=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__aer__6OpTestRC6OpTest(

OpTest &OpTest::operator<<=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__als__6OpTestRC6OpTest(

OpTest &OpTest::operator>>=(const OpTest &) { return *this; }
// CHECK: define {{.*}} @__ars__6OpTestRC6OpTest(

void OpTest::operator()() {}
// CHECK: define {{.*}} @__cl__6OpTest(

int OpTest::operator[](int x) { return x; }
// CHECK: define {{.*}} @__vc__6OpTesti(

namespace std {
  class StdClass {
  public:
    void std_method();
    bool operator==(const StdClass &) const;
  };
  void StdClass::std_method() {}
// CHECK: define {{.*}} @std_method__8StdClass(

  bool StdClass::operator==(const StdClass &) const { return true; }
// CHECK: define {{.*}} @__eq__C8StdClassRC8StdClass(

  void std_free_func() {}
// CHECK: define {{.*}} @std_free_func__Fv(
}

void test_const_ptr(int * const * p) {}
// CHECK: define {{.*}} @test_const_ptr__FPCPi(

void test_const_ptr_ref(int * const & p) {}
// CHECK: define {{.*}} @test_const_ptr_ref__FRCPi(

void test_cv_ptr(int * const volatile * p) {}
// CHECK: define {{.*}} @test_cv_ptr__FPCVPi(

class DerivedCheck;
typedef int DerivedCheck::*DerivedCheckPTMD;
typedef int (DerivedCheck::*DerivedCheckPTMF)();

void test_const_ptmd(DerivedCheckPTMD const * p) {}
// CHECK: define {{.*}} @test_const_ptmd__FPCPO12DerivedCheck_i(

void test_const_ptmd_ref(DerivedCheckPTMD const & p) {}
// CHECK: define {{.*}} @test_const_ptmd_ref__FRCPO12DerivedCheck_i(

void test_const_ptmf(DerivedCheckPTMF const * p) {}
// CHECK: define {{.*}} @test_const_ptmf__FPPM12DerivedCheckFP12DerivedCheck_i(

void test_top_level_const(int * const p, const int x) {}
// CHECK: define {{.*}} @test_top_level_const__FPii(

void test_complex(__complex__ double c) {}
// CHECK: define {{.*}} @test_complex__FJd(

void test_array_ref(int (&arr)[10]) {}
// CHECK: define {{.*}} @test_array_ref__FRA9_i(

void test_fn_ptr(int (*fn)(double)) {}
// CHECK: define {{.*}} @test_fn_ptr__FPFd_i(

void test_vu_ptr(volatile unsigned int * p) {}
// CHECK: define {{.*}} @test_vu_ptr__FPUVi(

void test_restrict_ptr(int * __restrict * p) {}
// CHECK: define {{.*}} @test_restrict_ptr__FPuPi(

struct S { int x; };

void test_struct_val(S s) {}
// CHECK: define {{.*}} @test_struct_val__FG1S(

void test_ptr_repeats(int *a, int *b, int *c) {}
// CHECK: define {{.*}} @test_ptr_repeats__FPiN20(

void test_builtin_repeats(int a, int b, int c) {}
// CHECK: define {{.*}} @test_builtin_repeats__Fiii(

void test_ref_repeats(int &a, int &b, int &c) {}
// CHECK: define {{.*}} @test_ref_repeats__FRiN20(

void test_mixed(int *a, int *b, float *c, int *d) {}
// CHECK: define {{.*}} @test_mixed__FPiT0PfT0(

void test_repeat_bools(bool a, bool b, bool c) {}
// CHECK: define {{.*}} @test_repeat_bools__FbN20(

void test_mixed_repeats(int a, int *b, int *c) {}
// CHECK: define {{.*}} @test_mixed_repeats__FiPiT1(

template <typename T, int N> class TemplateCheck { T val[N]; };

void test_template_ptr(TemplateCheck<int, 42> *p) {}
// CHECK: define {{.*}} @test_template_ptr__FPt13TemplateCheck2Zii42(

void test_template_val(TemplateCheck<float, 10> v) {}
// CHECK: define {{.*}} @test_template_val__FGt13TemplateCheck2Zfi10(

template <typename T> struct LitInner1 {};
template <typename T, typename U> struct LitInner2 {};
template <int N> struct LitInner3 {};

template <template <typename> class TT>
struct LitOuter1 {
  void foo() {}
};
template <template <typename, typename> class TT>
struct LitOuter2 {
  void foo() {}
};
template <template <int> class TT>
struct LitOuter3 {
  void foo() {}
};

void test_lit_template_template() {
  LitOuter1<LitInner1> o1;
  o1.foo();
  // CHECK: call {{.*}} @foo__t9LitOuter11z1Z9LitInner1(
  
  LitOuter2<LitInner2> o2;
  o2.foo();
  // CHECK: call {{.*}} @foo__t9LitOuter21z2ZZ9LitInner2(
  
  LitOuter3<LitInner3> o3;
  o3.foo();
  // CHECK: call {{.*}} @foo__t9LitOuter31z1i9LitInner3(
}

// --- New Backreference Indexing Branch Coverage Tests ---

struct Target {
  void test_method_backref(Target* a, Target* b);
  static void test_static_method_backref(Target* a, Target* b);
};
void Target::test_method_backref(Target* a, Target* b) {}
// CHECK: define {{.*}} @test_method_backref__6TargetP6TargetT1(
void Target::test_static_method_backref(Target* a, Target* b) {}
// CHECK: define {{.*}} @test_static_method_backref__6TargetP6TargetT1(


namespace InteropNS {
  struct Target {};
  void test_ns_backref(Target a, Target b, Target c) {}
  // CHECK: define {{.*}} @test_ns_backref__9InteropNSGQ29InteropNS6TargetN21(

  void test_ns_ptr_ref(const Target* a, Target& b) {}
  // CHECK: define {{.*}} @test_ns_ptr_ref__9InteropNSPCQ29InteropNS6TargetRQ29InteropNS6Target(
}

namespace std {
  void test_std_backref(InteropNS::Target a, InteropNS::Target b) {}
  // CHECK: define {{.*}} @test_std_backref__FGQ29InteropNS6TargetT0(
}

namespace std {
  namespace my_foo {
    void test_std_nested_backref(InteropNS::Target a, InteropNS::Target b) {}
    // CHECK: define {{.*}} @test_std_nested_backref__6my_fooGQ29InteropNS6TargetT1(
  }
}

// CHECK: define {{.*}} @__thunk_4_f2__7Derived(


