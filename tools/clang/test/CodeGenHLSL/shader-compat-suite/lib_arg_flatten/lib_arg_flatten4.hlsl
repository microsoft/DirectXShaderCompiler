// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure nested struct parameter not replaced as function call arg.

// CHECK: call void @"\01?test_extern@@YAMUFoo@@@Z"(float %{{.*}}, float* nonnull %{{.*}})

struct Foo {
  float a;
};

struct Bar {
  Foo foo;
  float b;
};

float test_extern(Foo foo);

float test(Bar b)
{
  float x = test_extern(b.foo);
  return x + b.b;
}
