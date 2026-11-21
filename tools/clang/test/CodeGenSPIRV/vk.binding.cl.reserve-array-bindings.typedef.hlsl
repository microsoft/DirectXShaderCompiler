// RUN: %dxc -T ps_6_0 -E main -fspv-reserve-resource-array-bindings -fcgl  %s -spirv | FileCheck %s

// Regression test: arrays declared through a typedef must reserve one binding
// per element, just like arrays spelled out directly. Both the explicitly and
// the implicitly bound typedef'd arrays have to be counted in full, otherwise
// the resources declared after them overlap.
// CHECK: OpDecorate %ExplicitTextures Binding 0
// CHECK: OpDecorate %ImplicitTextures Binding 3
// CHECK: OpDecorate %NextTexture Binding 6
typedef Texture2D Textures[3];
Textures     ExplicitTextures : register(t0);
Textures     ImplicitTextures;
Texture2D    NextTexture;
SamplerState MySampler;

float4 main(float2 TexCoord : TexCoord) : SV_Target0
{
  float4 result =
    ExplicitTextures[0].Sample(MySampler, TexCoord) +
    ExplicitTextures[2].Sample(MySampler, TexCoord) +
    ImplicitTextures[0].Sample(MySampler, TexCoord) +
    ImplicitTextures[2].Sample(MySampler, TexCoord) +
    NextTexture.Sample(MySampler, TexCoord);
  return result;
}
