// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm -o - %s | FileCheck %s

namespace NsJdzfC0 {
struct Czyah9C3kOSU {
  virtual void print_class() {}
  virtual void vf_NYi17f9Bv4() {}
  virtual ~Czyah9C3kOSU() {}
};
}

namespace NsUzC8cDj {
struct CGRQZo1C : public virtual NsJdzfC0::Czyah9C3kOSU {
  virtual void print_class() {}
  virtual void vf_OR26a() {}
  virtual ~CGRQZo1C() {}
};
}

namespace NsuPKHDHA2W4 {
struct CEvuYB4jl2Nl {
  virtual void print_class() {}
  virtual ~CEvuYB4jl2Nl() {}
};

struct CGu__3 : public NsUzC8cDj::CGRQZo1C, public NsuPKHDHA2W4::CEvuYB4jl2Nl {
  virtual void print_class() {}
  virtual ~CGu__3() {}
};
}

void test() {
  NsuPKHDHA2W4::CGu__3 obj;
}

// CHECK: @__vt_Q29NsUzC8cDj8CGRQZo1C = linkonce_odr constant [3 x ptr]
// CHECK-NOT: @__vt_Q212NsuPKHDHA2W46CGu__3 =
