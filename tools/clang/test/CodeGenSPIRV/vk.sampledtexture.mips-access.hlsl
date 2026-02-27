// RUN: %dxc -T ps_6_7 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK:  [[cu12:%[0-9]+]] = OpConstantComposite %v2uint %uint_1 %uint_2

// CHECK: [[type_1d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 0 0 1 Unknown
// CHECK: [[type_1d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image]]
// CHECK: [[type_1d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 1 0 1 Unknown
// CHECK: [[type_1d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image_array]]
// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]

vk::SampledTexture1D<float4> tex1d;
vk::SampledTexture1DArray<float4> tex1dArray;
vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;

void main() {
// CHECK:      [[pos1:%[0-9]+]] = OpLoad %v2uint %pos1
// CHECK-NEXT: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage [[type_2d_image]] [[tex1_load]]
// CHECK-NEXT:      [[result1:%[0-9]+]] = OpImageFetch %v4float [[tex_img]] [[pos1]] Lod %uint_2
// CHECK-NEXT:                    OpStore %a1 [[result1]]
  uint2 pos1 = uint2(1,2);
  float4 a1 = tex2d.mips[2][pos1];

// CHECK:    [[pos2:%[0-9]+]] = OpLoad %v3uint %pos2
// CHECK-NEXT: [[load_arr:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK-NEXT: [[img_arr:%[a-zA-Z0-9_]+]] = OpImage [[type_2d_image_array]] [[load_arr]]
// CHECK-NEXT: [[res_arr:%[0-9]+]] = OpImageFetch %v4float [[img_arr]] [[pos2]] Lod %uint_3
  uint3 pos2 = uint3(1, 2, 4);
  float4 a2 = tex2dArray.mips[3][pos2];

// CHECK: [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK-NEXT: [[tex1d_img:%[a-zA-Z0-9_]+]] = OpImage [[type_1d_image]] [[tex1d_load]]
// CHECK-NEXT: [[result1d:%[0-9]+]] = OpImageFetch %v4float [[tex1d_img]] %uint_1 Lod %uint_2
  float4 a3 = tex1d.mips[2][1];

// CHECK:      [[pos1:%[0-9]+]] = OpLoad %v2uint %pos1
// CHECK-NEXT: [[tex1d_arr_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK-NEXT: [[tex1d_arr_img:%[a-zA-Z0-9]+]] = OpImage [[type_1d_image_array]] [[tex1d_arr_load]]
// CHECK-NEXT: [[result1d_arr:%[0-9]+]] = OpImageFetch %v4float [[tex1d_arr_img]] [[pos1]] Lod %uint_3
  float4 a4 = tex1dArray.mips[3][pos1];
}
