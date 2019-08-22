// Run: %dxc -T ps_6_0 -E main -fspv-flatten-resource-arrays

// CHECK: error: ran into binding number conflict when assigning binding number 3 in set 0

Texture2D    MyTextures[5] : register(t0); // Forced use of binding numbers 0, 1, 2, 3, 4.
Texture2D    AnotherTexture : register(t3); // Error: Forced use of binding number 3.
SamplerState MySampler;

float4 main(float2 TexCoord : TexCoord) : SV_Target0 {
  float4 result =
    MyTextures[0].Sample(MySampler, TexCoord) +
    MyTextures[1].Sample(MySampler, TexCoord) +
    MyTextures[2].Sample(MySampler, TexCoord) +
    MyTextures[3].Sample(MySampler, TexCoord) +
    MyTextures[4].Sample(MySampler, TexCoord) +
    AnotherTexture.Sample(MySampler, TexCoord);
  return result;
}

