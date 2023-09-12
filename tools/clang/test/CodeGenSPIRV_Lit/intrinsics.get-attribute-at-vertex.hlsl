// RUN: not %dxc -T ps_6_1 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

struct PSInput {
  float4 position : SV_POSITION;
  nointerpolation float3 color : COLOR;
};

cbuffer constants : register(b0) {
  float4 g_constants;
}

float4 main(PSInput input) : SV_TARGET {
  uint cmp = (uint)(g_constants[0]);

  // CHECK: 16:21: error: GetAttributeAtVertex intrinsic function unimplemented
  float colorAtV0 = GetAttributeAtVertex(input.color, 0)[cmp];

  // CHECK: 19:21: error: GetAttributeAtVertex intrinsic function unimplemented
  float colorAtV1 = GetAttributeAtVertex(input.color, 1)[cmp];

  // CHECK: 22:21: error: GetAttributeAtVertex intrinsic function unimplemented
  float colorAtV2 = GetAttributeAtVertex(input.color, 2)[cmp];

  return float4(colorAtV0, colorAtV1, colorAtV2, 0);
}
