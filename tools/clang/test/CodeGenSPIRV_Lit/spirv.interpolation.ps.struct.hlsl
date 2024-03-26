// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

struct PSInput
{
  float4 position : SV_POSITION;
  nointerpolation float3 color : COLOR;
};

cbuffer constants : register(b0)
{
  float4 g_constants;
}

float4 main(PSInput input) : SV_TARGET
{
  uint cmp = (uint)(g_constants[0]);

  float colorAtV0 = GetAttributeAtVertex(input.color, 0)[cmp];
  float colorAtV1 = GetAttributeAtVertex(input.color, 1)[cmp];
  float colorAtV2 = GetAttributeAtVertex(input.color, 2)[cmp];

  return float4(colorAtV0, colorAtV1, colorAtV2, 0);
}

// CHECK: OpDecorate [[var:%[a-zA-Z0-9_]+]] Flat
// CHECK: OpDecorate [[var]] PerVertexKHR

// CHECK: [[array:%[a-zA-Z0-9_]+]] = OpTypeArray %v3float %uint_3
// CHECK:  [[type:%[a-zA-Z0-9_]+]] = OpTypePointer Input [[array]]
// CHECK:                  [[var]] = OpVariable [[type]] Input
