// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

class PolyBase {
public:
  virtual ~PolyBase();
  virtual int poly_func();
};

class IntermediateVBase : virtual public PolyBase {
public:
  int i;
  virtual ~IntermediateVBase();
  virtual int inter_func();
};

class DeepVSub : virtual public IntermediateVBase {
public:
  int d;
  DeepVSub();
  virtual ~DeepVSub();
};

// CHECK: declare noundef ptr @__8DeepVSubi(

void test() {
  DeepVSub obj;
}
