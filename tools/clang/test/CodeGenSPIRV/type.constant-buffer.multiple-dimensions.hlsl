// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=universal1.5

struct Foo {
  float4 a;
  int2 b;
};

// CHECK: [[s01:%\w+]] = OpTypeStruct %v4float %v2int
// CHECK: [[s02:%\w+]] = OpTypeArray [[s01]] %uint_3
// CHECK: [[s03:%\w+]] = OpTypeArray [[s02]] %uint_2
// CHECK: [[s04:%\w+]] = OpTypePointer Uniform [[s03]]
ConstantBuffer<Foo> myCB2[2][3] : register(b0, space1);

struct VSOutput {
  float2 TexCoord : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET {
  // CHECK: [[s05:%\w+]] = OpTypePointer Uniform %v4float
  // CHECK: [[s06:%\w+]] = OpVariable [[s04:%\w+]] Uniform
  // CHECK: OpAccessChain [[s05]] [[s06]] %int_1 %int_0 %int_0
  return float4(1.0, 1.0, 1.0, 1.0) * myCB2[1][0].a;
}
