// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base {
  virtual void print_class() {}
  virtual int print_class_val() { return 1; }
  virtual ~Base() {}
  Base() {}
};

struct CDerived : public Base {
  virtual void print_class() {}
  virtual int print_class_val() { return 2; }
  virtual ~CDerived() {}
  CDerived() {}
};

struct VDerived : public virtual CDerived {
  virtual void print_class() {}
  virtual int print_class_val() { return 3; }
  virtual ~VDerived() {}
  VDerived() {}
};

struct CpDerived : public VDerived {
  virtual void print_class() {}
  virtual int print_class_val() { return 4; }
  virtual ~CpDerived() {}
  CpDerived() {}
};

// CHECK: @__vt_9CpDerived.8CDerived = linkonce_odr constant

int test() {
  CpDerived obj;
  Base *b = (Base*)&obj;
  return b->print_class_val();
}
