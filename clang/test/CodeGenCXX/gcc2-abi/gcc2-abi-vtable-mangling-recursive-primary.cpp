// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -fno-rtti -emit-llvm -o - %s | FileCheck %s

namespace Nsv5EZcPiEjOD {
struct COKMylM {
  int f_TrAf;
  long long f_pC07tnHtl;
  virtual void print_class() {}
  virtual void vf_xXm5() {}
  virtual ~COKMylM() {}
};

struct CmsrIuXC : public virtual Nsv5EZcPiEjOD::COKMylM {
  char f_anIwJ;
  int f_r7TZsK;
  virtual void print_class() {}
  virtual void vf_ktN4v() {}
  virtual ~CmsrIuXC() {}
};
}

struct CvU2HtXujX2g {
  char f_DT2Hhat6cR;
  virtual void print_class() {}
  virtual void vf_v3cgHXQVtbK() {}
  virtual ~CvU2HtXujX2g() {}
};

struct Cb4RdgydfBzl : public Nsv5EZcPiEjOD::CmsrIuXC, public CvU2HtXujX2g {
  short f_T96WW;
  long long f_Kf_Z5S;
  virtual void print_class() {}
  virtual ~Cb4RdgydfBzl() {}
};

void test() {
  Cb4RdgydfBzl obj;
}

// CHECK: define {{.*}} @__base_dtor._._12Cb4RdgydfBzl
// CHECK: store ptr @__vt_Q213Nsv5EZcPiEjOD8CmsrIuXC, ptr %vfptr
// CHECK-NOT: store ptr @__vt_12Cb4RdgydfBzl,
