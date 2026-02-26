// RUN: %dxc -T ps_6_8 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpCapability ImageQuery

vk::SampledTexture2D<float4> t1 : register(t0);

// CHECK: %type_2d_image = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: %type_sampled_image = OpTypeSampledImage %type_2d_image
// CHECK: [[ptr:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant %type_sampled_image

// CHECK: %t1 = OpVariable [[ptr]] UniformConstant

void main() {
  float2 xy = float2(0.5, 0.5);

//CHECK:          [[tex1:%[a-zA-Z0-9_]+]] = OpLoad %type_sampled_image %t1
//CHECK-NEXT:    [[xy_load:%[a-zA-Z0-9_]+]] = OpLoad %v2float %xy
//CHECK-NEXT: [[query:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex1]] [[xy_load]]
//CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query]] 1
  float lod1 = t1.CalculateLevelOfDetailUnclamped(xy);
}