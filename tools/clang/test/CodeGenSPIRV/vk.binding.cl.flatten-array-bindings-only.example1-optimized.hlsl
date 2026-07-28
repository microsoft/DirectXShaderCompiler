// RUN: %dxc -T ps_6_0 -E main -fspv-flatten-resource-array-bindings-only -O3  %s -spirv | FileCheck %s

// The array is NOT split into individual variables (unlike
// -fspv-flatten-resource-arrays): MyTextures/MySamplers stay as single
// array variables, each with a single Binding decoration, but the binding
// numbers reserved for them still account for the full array size.
// CHECK: OpDecorate %MyTextures Binding 0
// CHECK: OpDecorate %AnotherTexture Binding 5
// CHECK: OpDecorate %NextTexture Binding 6
// CHECK: OpDecorate %MySamplers Binding 7
// CHECK-NOT: MyTextures_0_
// CHECK-NOT: MySamplers_0_
Texture2D    MyTextures[5] : register(t0);
Texture2D    NextTexture;  // This is suppose to be t6.
Texture2D    AnotherTexture : register(t5);
SamplerState MySamplers[2];

[unroll]
float4 main(float2 TexCoord : TexCoord) : SV_Target0
{
  float4 result = 0;
  for (uint i = 0; i < 5; ++i)
    result += MyTextures[i].Sample(MySamplers[i < 3 ? 0 : 1], TexCoord);
  result += AnotherTexture.Sample(MySamplers[1], TexCoord);
  result += NextTexture.Sample(MySamplers[1], TexCoord);
  return result;
}
