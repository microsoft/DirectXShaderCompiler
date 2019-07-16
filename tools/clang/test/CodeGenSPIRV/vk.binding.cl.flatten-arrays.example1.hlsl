// Run: %dxc -T ps_6_0 -E main -fspv-flatten-resource-arrays

// CHECK: OpDecorate %MyTextures Binding 0
// CHECK: OpDecorate %AnotherTexture Binding 5
// CHECK: OpDecorate %NextTexture Binding 6
// CHECK: OpDecorate %MySampler Binding 7
Texture2D    MyTextures[5] : register(t0);
Texture2D    NextTexture;  // This is suppose to be t6.
Texture2D    AnotherTexture : register(t5);
SamplerState MySampler;

float4 main(float2 TexCoord : TexCoord) : SV_Target0
{
  float4 result =
    MyTextures[0].Sample(MySampler, TexCoord) +
    MyTextures[1].Sample(MySampler, TexCoord) +
    MyTextures[2].Sample(MySampler, TexCoord) +
    MyTextures[3].Sample(MySampler, TexCoord) +
    MyTextures[4].Sample(MySampler, TexCoord) +
    AnotherTexture.Sample(MySampler, TexCoord) +
    NextTexture.Sample(MySampler, TexCoord);
  return result;
}
