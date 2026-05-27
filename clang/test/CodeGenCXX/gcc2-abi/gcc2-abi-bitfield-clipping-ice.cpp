// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o - | FileCheck %s

struct Base {
  char* data[6];
  virtual void print() {}
  virtual ~Base() {}
};

struct MidNonVirt : public Base {
  bool bit_1 : 1;
  Base** ptr_base;
  bool* bool_ptr;
  short* short_ptr;
  virtual ~MidNonVirt() {}
};

struct MidVirt : public virtual MidNonVirt {
  bool*** ptr_ptr_ptr;
  double double_field;
  bool bit_2 : 1;
  virtual ~MidVirt() {}
};

struct MostDerived : public virtual MidVirt {
  unsigned char char_field;
  unsigned int bit_3 : 17;
  virtual ~MostDerived() {}
};

// Trigger record lowering phase during CodeGen
MostDerived g_obj;

// CHECK: define {{.*}} @get_size_most_derived
// CHECK: ret i32 72
int get_size_most_derived() { return sizeof(MostDerived); }
