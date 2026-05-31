// RUN: %clang_cc1 -triple i386-unknown-linux-gnu -fc++-abi=gcc2 -emit-llvm %s -o -

struct Foo {
  ~Foo() {}
};

void test(Foo* p) {
  p->~Foo();
}

// CHECK: define dso_local void @test__FP3Foo(ptr noundef %p)
// CHECK: %[[P:.*]] = load ptr, ptr %p.addr, align 4
// CHECK: call noundef ptr @_._3Foo(ptr noundef nonnull align 1 dereferenceable(1) %[[P]], i32 noundef 2)
