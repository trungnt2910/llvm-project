// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Abstract {
  virtual void foo() = 0;
  virtual void bar();
};

void Abstract::bar() {}

// CHECK: @__vt_8Abstract = {{.*}}constant [4 x ptr] [ptr null, ptr @__tf8Abstract, ptr @__pure_virtual, ptr @bar__8Abstract]
// CHECK: declare void @__pure_virtual()
