// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -default-linkage external %s | FileCheck %s

// Make sure globals for resource exist.
// CHECK: @"\01?g_txDiffuse@@3V?$Texture2D@V?$vector@M$03@@@@A" = external constant %"class.Texture2D<vector<float, 4> >", align 4
// CHECK: @"\01?g_samLinear@@3USamplerState@@A" = external constant %struct.SamplerState, align 4

Texture2D    g_txDiffuse;
SamplerState    g_samLinear;

float4 test(float2 c : C) : SV_TARGET
{
  float4 x = g_txDiffuse.Sample( g_samLinear, c );
  return x;
}

