// RUN: %dxc -T ps_6_0 -E main -fspv-implicit-hlsl-resource-arrays -fcgl  %s -spirv | FileCheck %s

// CHECK: OpDecorate %AnotherTexture Binding 0
// CHECK: OpDecorate %MyTexturesExplicit Binding 1
// CHECK: OpDecorate %MyTextures Binding 2
// CHECK: OpDecorate %MySamplersExplicit Binding 7
// CHECK: OpDecorate %MySamplers Binding 8
// CHECK: OpDecorate NextSampler Binding 10
Texture2D    AnotherTexture;
Texture2D    MyTexturesExplicit[5] : register(t1);
Texture2D    MyTextures[5];
SamplerState MySamplersExplicit[2] : register(s7);
SamplerState MySamplers[2];
SamplerState NextSampler;

float4 main(float2 TexCoord : TexCoord) : SV_Target0
{
  float4 result =
    MyTextures[0].Sample(MySamplers[0], TexCoord) +
    MyTextures[1].Sample(MySamplers[0], TexCoord) +
    MyTextures[2].Sample(MySamplers[0], TexCoord) +
    MyTextures[3].Sample(MySamplers[1], TexCoord) +
    MyTextures[4].Sample(MySamplersExplicit[1], TexCoord) +
    AnotherTexture.Sample(MySamplers[1], TexCoord) +
    MyTexturesExplicit[0].Sample(NextSampler, TexCoord);
  return result;
}
