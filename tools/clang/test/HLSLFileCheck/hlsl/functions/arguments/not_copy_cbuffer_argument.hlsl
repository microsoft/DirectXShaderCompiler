// RUN: %dxc -E main -Tps_6_0 -fcgl %s | FileCheck %s


// Make sure no memcpy generated.
// CHECK:@main
// CHECK-NOT:memcpy

struct Data
{
 float4 f[64];
};

cbuffer A {
  Data a;
};

float4 foo(Data d, int i) {
  return d.f[i];
}

float4 main(int i:I) :SV_Target {
  return foo(a, i);
}