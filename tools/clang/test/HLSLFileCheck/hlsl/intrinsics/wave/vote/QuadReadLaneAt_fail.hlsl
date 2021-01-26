// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: quadLaneId for QuadReadLaneAt must be a constnat value from 0 and 3

SamplerState samp0 : register(s0);
Texture2D tex0 : register(t0);

[RootSignature("DescriptorTable(SRV(t0)),DescriptorTable(Sampler(s0))")]
float3 main(float3 foo : FOO) : SV_TARGET {
  float3 f3 = foo;

  [loop]
  for (uint i = 0; i < 4; ++i)
  {
    f3 = QuadReadLaneAt(f3, i);
  }

  return f3;
}

