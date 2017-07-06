// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: fadd

float noise(float2 s) {
  return s.x + s.y;
}

float4 main(float4 a : A) : SV_TARGET
{
  return noise(a.xz);
}