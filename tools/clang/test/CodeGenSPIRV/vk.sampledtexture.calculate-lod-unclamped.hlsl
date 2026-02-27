// RUN: %dxc -T ps_6_8 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpCapability ImageQuery

// CHECK: [[type_1d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 0 0 1 Unknown
// CHECK: [[type_1d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image]]
// CHECK: [[type_1d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 1 0 1 Unknown
// CHECK: [[type_1d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image_array]]
// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]
// CHECK: [[type_3d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 3D 0 0 0 1 Unknown
// CHECK: [[type_3d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_3d_image]]

vk::SampledTexture1D<float4> tex1d;
vk::SampledTexture1DArray<float4> tex1dArray;
vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;
vk::SampledTexture3D<float4> tex3d;

void main() {
  float2 xy = float2(0.5, 0.5);

//CHECK:          [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
//CHECK-NEXT:    [[xy_load:%[a-zA-Z0-9_]+]] = OpLoad %v2float %xy
//CHECK-NEXT: [[query:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex1_load]] [[xy_load]]
//CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query]] 1
  float lod1 = tex2d.CalculateLevelOfDetailUnclamped(xy);

//CHECK:          [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
//CHECK-NEXT:    [[xy_load_2:%[a-zA-Z0-9_]+]] = OpLoad %v2float %xy
//CHECK-NEXT: [[query2:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex2_load]] [[xy_load_2]]
//CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query2]] 1
  float lod2 = tex2dArray.CalculateLevelOfDetailUnclamped(xy);

// CHECK:          [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK-NEXT: [[query1d:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex1d_load]] %float_0_5
// CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query1d]] 1
  float lod3 = tex1d.CalculateLevelOfDetailUnclamped(0.5);

// CHECK:             [[tex1da_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK-NEXT: [[query1da:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex1da_load]] %float_0_5
// CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query1da]] 1
  float lod4 = tex1dArray.CalculateLevelOfDetailUnclamped(0.5);

// CHECK:          [[tex3d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_3d_sampled_image]] %tex3d
// CHECK-NEXT: [[query3d:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex3d_load]] %{{[0-9]+}}
// CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query3d]] 1
  float lod5 = tex3d.CalculateLevelOfDetailUnclamped(float3(0.5, 0.25, 0));
}