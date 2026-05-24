// RUN: %clang_cc1 -triple i386-pc-linux-gnu -fc++-abi=gcc2 -emit-llvm -o - %s | FileCheck %s

struct VBase {
  virtual ~VBase();
};

struct Derived : virtual VBase {
  virtual ~Derived() {} // Inline virtual destructor!
};

void test() {
  Derived *d = new Derived();
  VBase *b = d;
  delete b;
}

// Check that the virtual destructor thunk for VBase in Derived's vtable actually targets the Wrapper _._7Derived (not the renamed internal __base_dtor).
// CHECK: define linkonce_odr noundef ptr @__thunk_4__._7Derived(ptr noundef %this, i32 noundef %__in_chrg)
// CHECK: %[[REG:.*]] = getelementptr inbounds i8, ptr %{{.*}}, i32 -4
// CHECK: %{{.*}} = tail call noundef ptr @_._7Derived(ptr noundef nonnull align 4 dereferenceable(4) %[[REG]], i32 noundef %{{.*}})

// Check that the destructor wrapper is generated correctly.
// CHECK: define linkonce_odr noundef ptr @_._7Derived(ptr noundef nonnull align 4 dereferenceable(4) %this, i32 noundef %in_chrg)
