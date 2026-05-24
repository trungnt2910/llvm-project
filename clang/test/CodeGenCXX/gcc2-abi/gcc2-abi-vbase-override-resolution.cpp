// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base {
  virtual void print_class();
  virtual ~Base();
  Base();
};

struct CDerived : public Base {
  virtual void print_class();
  virtual ~CDerived();
  CDerived();
};

struct VDerived : public virtual CDerived {
  virtual void print_class();
  virtual ~VDerived();
  VDerived();
};

struct CpDerived : public VDerived {
  virtual void print_class();
  virtual ~CpDerived();
  CpDerived();
};

void test_virt_override() {
  CpDerived obj;
}

// CHECK-LABEL: define dso_local void @test_virt_override__Fv()
// CHECK: call noundef ptr @__9CpDerivedi(
