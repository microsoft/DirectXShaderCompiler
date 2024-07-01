// RUN: not %dxc -T ps_6_6 -E main -fcgl %s -spirv  2>&1 | FileCheck %s

// CHECK: error: Using SamplerDescriptorHeap/ResourceDescriptorHeap & implicit bindings is disallowed.
Texture2D Texture;

float4 main() : SV_Target {
  SamplerState Sampler = SamplerDescriptorHeap[0];
  return Texture.Sample(Sampler, float2(0, 0));
}
