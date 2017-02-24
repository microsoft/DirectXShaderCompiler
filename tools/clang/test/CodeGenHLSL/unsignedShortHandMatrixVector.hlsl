// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: uint2
// CHECK: uint4x4

unsigned int2 a;
unsigned int4x4 b;

float4 main() : SV_Target 
{
  return a.x + b[1][0];
}
