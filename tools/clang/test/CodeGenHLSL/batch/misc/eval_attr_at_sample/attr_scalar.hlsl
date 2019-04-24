// RUN: %dxc -T ps_6_0 -E main %s -O3 | FileCheck %s

// CHECK: @main()

Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

float4 main(float u : TEXCOORD0, float v : TEXCOORD1) : SV_TARGET {
  float4 result = float4(0,0,0,0);
  int i = 0;
  [branch]
  if (u > 0.5) {
    float2 coord = float2(
      EvaluateAttributeAtSample(u, i),
      EvaluateAttributeAtSample(v, i)
    );
    result += tex0.Sample(samp0, coord);
  }
  return result;
}



