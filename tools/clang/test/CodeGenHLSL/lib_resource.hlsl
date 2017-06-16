// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure globals for link info exist.
// CHECK: g_txDiffuse_rangeID
// CHECK: g_samLinear_rangeID

// Make sure link info metadata exist.
// CHECK: dx.resources.link.info
// CHECK: !{i32* @g_txDiffuse_rangeID}
// CHECK: !{i32* @g_samLinear_rangeID}


Texture2D    g_txDiffuse;
SamplerState    g_samLinear;

float4 test(float2 c : C) : SV_TARGET
{
  float4 x = g_txDiffuse.Sample( g_samLinear, c );
  return x;
}

