// Run: %dxc -T ps_6_0 -E main -fvk-bind-register s10 5t 10 1

Texture2D MyTexture;
SamplerState MySampler;

float4 main() : SV_Target {
  return MyTexture.Sample(MySampler, float2(0.1, 0.2));
}

// CHECK: error: invalid -fvk-bind-register space number: 5t
