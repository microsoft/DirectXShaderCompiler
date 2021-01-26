// RUN: %dxc -E main -T ps_6_0 -O0 %s | FileCheck %s

// Make sure the lane id operand is actually resolved to a constant.

// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 0
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 0
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 0

// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 1
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 1
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 1

// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 2
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 2
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 2

// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 3
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 3
// CHECK: call float @dx.op.quadReadLaneAt.f32(i32 122, float %{{.+}}, i32 3

SamplerState samp0 : register(s0);
Texture2D tex0 : register(t0);

[RootSignature("DescriptorTable(SRV(t0)),DescriptorTable(Sampler(s0))")]
float3 main(float3 foo : FOO) : SV_TARGET {
  float3 f3 = foo;
  [unroll]
  for (uint i = 0; i < 4; ++i)
  {
    f3 = QuadReadLaneAt(f3, i);
  }

  return f3;
}
