// RUN: %dxc -E main -T ps_6_0 %s -Gec | FileCheck %s

// CHECK: error: identifier is unsupported in HLSL

float4 main(float4 color : COLOR, float vface : VFACE) : SV_TARGET
{
  return (vface > 0) ? color : (color*2);
}
