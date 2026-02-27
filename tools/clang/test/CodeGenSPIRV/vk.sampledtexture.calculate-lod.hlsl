// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpCapability ImageQuery

// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]

vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;

void main() {
  float2 xy = float2(0.5, 0.5);

//CHECK:          [[tex1:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
//CHECK-NEXT:    [[xy_load:%[a-zA-Z0-9_]+]] = OpLoad %v2float %xy
//CHECK-NEXT: [[query:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex1]] [[xy_load]]
//CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query]] 0
  float lod = tex2d.CalculateLevelOfDetail(xy);

//CHECK:          [[tex2:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
//CHECK-NEXT:    [[xy_load_2:%[a-zA-Z0-9_]+]] = OpLoad %v2float %xy
//CHECK-NEXT: [[query2:%[a-zA-Z0-9_]+]] = OpImageQueryLod %v2float [[tex2]] [[xy_load_2]]
//CHECK-NEXT:        {{%[0-9]+}} = OpCompositeExtract %float [[query2]] 0
  float lod2 = tex2dArray.CalculateLevelOfDetail(xy);
}
