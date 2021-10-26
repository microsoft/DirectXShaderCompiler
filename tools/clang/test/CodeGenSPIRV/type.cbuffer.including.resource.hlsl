// Run: %dxc -T ps_6_0 -E main

// CHECK: OpDecorate %MyCbuffer DescriptorSet 0
// CHECK: OpDecorate %MyCbuffer Binding 1
// CHECK: OpDecorate %AnotherCBuffer DescriptorSet 0
// CHECK: OpDecorate %AnotherCBuffer Binding 2
// CHECK: OpDecorate %y DescriptorSet 0
// CHECK: OpDecorate %y Binding 0
// CHECK: OpDecorate %z DescriptorSet 0
// CHECK: OpDecorate %z Binding 3
// CHECK: OpDecorate %w DescriptorSet 0
// CHECK: OpDecorate %w Binding 4
// CHECK: OpMemberDecorate %type_MyCbuffer 0 Offset 0

// CHECK: %type_MyCbuffer = OpTypeStruct %v4float
// CHECK: %type_AnotherCBuffer = OpTypeStruct

// CHECK:           %y = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK:           %z = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK:   %MyCbuffer = OpVariable %_ptr_Uniform_type_MyCbuffer Uniform
// CHECK:           %w = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %AnotherCBuffer = OpVariable %_ptr_Uniform_type_AnotherCBuffer Uniform

cbuffer MyCbuffer : register(b1) {
  float4 x;
  Texture2D y;
  SamplerState z;
};

cbuffer AnotherCBuffer : register(b2) {
  SamplerState w;
}

float4 main(float2 uv : TEXCOORD) : SV_TARGET {
// CHECK: [[ptr_x:%\d+]] = OpAccessChain %_ptr_Uniform_v4float %MyCbuffer %int_0
// CHECK: [[x:%\d+]] = OpLoad %v4float [[ptr_x]]

// CHECK: [[y:%\d+]] = OpLoad %type_2d_image %y
// CHECK: [[z:%\d+]] = OpLoad %type_sampler %z
// CHECK: [[yz:%\d+]] = OpSampledImage %type_sampled_image [[y]] [[z]]
// CHECK: [[sample0:%\d+]] = OpImageSampleImplicitLod %v4float [[yz]]
// CHECK: [[add0:%\d+]] = OpFAdd %v4float [[x]] [[sample0]]

// CHECK: [[y:%\d+]] = OpLoad %type_2d_image %y
// CHECK: [[w:%\d+]] = OpLoad %type_sampler %w
// CHECK: [[yw:%\d+]] = OpSampledImage %type_sampled_image [[y]] [[w]]
// CHECK: [[sample1:%\d+]] = OpImageSampleImplicitLod %v4float [[yw]]
// CHECK: OpFAdd %v4float [[add0]] [[sample1]]
  return x + y.Sample(z, uv) + y.Sample(w, uv);
}
