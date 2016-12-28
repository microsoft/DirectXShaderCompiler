// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s

struct Foo {
  float4 f;
};

typedef Foo FooA[2];

// CHECK: error: array cannot be used as a type parameter
ConstantBuffer<FooA> CB1;

// CHECK: error: array cannot be used as a type parameter
ConstantBuffer<FooA> CB[4][3];
// CHECK: error: array cannot be used as a type parameter
TextureBuffer<FooA> TB[4][3];

float4 main(int a : A) : SV_Target
{
  return CB[3][2][1].f * TB[3][2][1].f;
}
