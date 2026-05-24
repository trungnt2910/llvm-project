// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base0 {
  char f0;
  double f1;
  virtual void print_class() {}
  virtual ~Base0() {}
};

struct Base1 : public Base0 {
  virtual void print_class() {}
  virtual ~Base1() {}
};

struct Base2 : public virtual Base0 {
  int f2;
  virtual void print_class() {}
  virtual ~Base2() {}
};

// Derived has Base2 at offset 0, and Base1 at offset 12.
// Base1 is the primary base and has vptr at offset 12 of Base1 (offset 24 of Derived).
struct Derived : public Base2, public Base1 {
  virtual void print_class() {}
  virtual ~Derived() {}
};

void test(Derived *p) {
  // CHECK: %[[VFPR:.*]] = getelementptr inbounds i8, ptr %[[P:.*]], i32 20
  // CHECK: %[[VTABLE:.*]] = load ptr, ptr %[[VFPR]]
  // CHECK: %[[SLOT3:.*]] = getelementptr inbounds ptr, ptr %[[VTABLE]], i64 2
  // CHECK: %[[FUNC:.*]] = load ptr, ptr %[[SLOT3]]
  // CHECK: call void %[[FUNC]]
  p->print_class();
}
