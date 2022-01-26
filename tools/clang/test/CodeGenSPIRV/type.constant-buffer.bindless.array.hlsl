// RUN: %dxc -T vs_6_0 -E main

// Used for ConstantBuffer definition.
// CHECK: OpTypeStruct %mat4v4float
// Used for PerDraw local variable.
// CHECK: OpTypeStruct %mat4v4float

struct PerDraw {
  float4x4 Transform;
};

// Used for ConstantBuffer to PerDraw type cast.
// CHECK:     [[type_PerDraw:%\w+]] = OpTypeStruct %mat4v4float
// CHECK: [[ptr_type_PerDraw:%\w+]] = OpTypePointer Uniform [[type_PerDraw]]

ConstantBuffer<PerDraw> PerDraws[];

struct VSInput {
  float3 Position : POSITION;

  [[vk::builtin("DrawIndex")]]
  uint DrawIdx    : DRAWIDX;
};

float4 main(in VSInput input) : SV_POSITION {
// CHECK: [[ptr:%\w+]] = OpAccessChain [[ptr_type_PerDraw]] %PerDraws
// CHECK:                OpLoad [[type_PerDraw]] [[ptr]]
  const PerDraw perDraw = PerDraws[input.DrawIdx];
  return mul(float4(input.Position, 1.0f), perDraw.Transform);
}
