// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s
// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv -DERROR 2>&1 | FileCheck %s --check-prefix=ERROR

// CHECK: OpCapability ImageQuery

// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]

vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;

void main() {
  uint mipLevel = 1;
  uint width, height, numLevels, elements;

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT:                      OpStore %width [[query1_0]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT:                      OpStore %height [[query1_1]]
  tex2d.GetDimensions(width, height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:       [[mip:%[0-9]+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query2:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image2]] [[mip]]
// CHECK-NEXT: [[query2_0:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 0
// CHECK-NEXT:                      OpStore %width [[query2_0]]
// CHECK-NEXT: [[query2_1:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 1
// CHECK-NEXT:                      OpStore %height [[query2_1]]
// CHECK-NEXT:   [[query_level_2:%[0-9]+]] = OpImageQueryLevels %uint [[image2]]
// CHECK-NEXT:                      OpStore %numLevels [[query_level_2]]
  tex2d.GetDimensions(mipLevel, width, height, numLevels);

// CHECK:        [[t2_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK-NEXT:   [[image3:%[0-9]+]] = OpImage [[type_2d_image_array]] [[t2_load]]
// CHECK-NEXT:   [[query3:%[0-9]+]] = OpImageQuerySizeLod %v3uint [[image3]] %int_0
// CHECK-NEXT: [[query3_0:%[0-9]+]] = OpCompositeExtract %uint [[query3]] 0
// CHECK-NEXT:                      OpStore %width [[query3_0]]
// CHECK-NEXT: [[query3_1:%[0-9]+]] = OpCompositeExtract %uint [[query3]] 1
// CHECK-NEXT:                      OpStore %height [[query3_1]]
// CHECK-NEXT: [[query3_2:%[0-9]+]] = OpCompositeExtract %uint [[query3]] 2
// CHECK-NEXT:                      OpStore %elements [[query3_2]]
  tex2dArray.GetDimensions(width, height, elements);

// CHECK:        [[t2_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK-NEXT:   [[image4:%[0-9]+]] = OpImage %type_2d_image_array [[t2_load]]
// CHECK-NEXT:       [[mip:%[0-9]+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query4:%[0-9]+]] = OpImageQuerySizeLod %v3uint [[image4]] [[mip]]
// CHECK-NEXT: [[query4_0:%[0-9]+]] = OpCompositeExtract %uint [[query4]] 0
// CHECK-NEXT:                      OpStore %width [[query4_0]]
// CHECK-NEXT: [[query4_1:%[0-9]+]] = OpCompositeExtract %uint [[query4]] 1
// CHECK-NEXT:                      OpStore %height [[query4_1]]
// CHECK-NEXT: [[query4_2:%[0-9]+]] = OpCompositeExtract %uint [[query4]] 2
// CHECK-NEXT:                      OpStore %elements [[query4_2]]
// CHECK-NEXT:   [[query_level_4:%[0-9]+]] = OpImageQueryLevels %uint [[image4]]
// CHECK-NEXT:                      OpStore %numLevels [[query_level_4]]
  tex2dArray.GetDimensions(mipLevel, width, height, elements, numLevels);

  float f_width, f_height, f_numLevels;
// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT: [[f_query1_0:%[0-9]+]] = OpConvertUToF %float [[query1_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query1_0]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT: [[f_query1_1:%[0-9]+]] = OpConvertUToF %float [[query1_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query1_1]]
  tex2d.GetDimensions(f_width, f_height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:       [[mip:%[0-9]+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query2:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image2]] [[mip]]
// CHECK-NEXT: [[query2_0:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 0
// CHECK-NEXT: [[f_query2_0:%[0-9]+]] = OpConvertUToF %float [[query2_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query2_0]]
// CHECK-NEXT: [[query2_1:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 1
// CHECK-NEXT: [[f_query2_1:%[0-9]+]] = OpConvertUToF %float [[query2_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query2_1]]
// CHECK-NEXT:   [[query_level_2:%[0-9]+]] = OpImageQueryLevels %uint [[image2]]
// CHECK-NEXT: [[f_query_level_2:%[0-9]+]] = OpConvertUToF %float [[query_level_2]]
// CHECK-NEXT:                      OpStore %f_numLevels [[f_query_level_2]]
  tex2d.GetDimensions(mipLevel, f_width, f_height, f_numLevels);

  int i_width, i_height, i_numLevels;
// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT:  [[query_0_int:%[0-9]+]] = OpBitcast %int [[query1_0]]
// CHECK-NEXT:                      OpStore %i_width [[query_0_int]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT:  [[query_1_int:%[0-9]+]] = OpBitcast %int [[query1_1]]
// CHECK-NEXT:                      OpStore %i_height [[query_1_int]]
  tex2d.GetDimensions(i_width, i_height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage [[type_2d_image]] [[t1_load]]
// CHECK-NEXT:       [[mip:%[0-9]+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query2:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image2]] [[mip]]
// CHECK-NEXT: [[query2_0:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 0
// CHECK-NEXT:  [[query_0_int:%[0-9]+]] = OpBitcast %int [[query2_0]]
// CHECK-NEXT:                      OpStore %i_width [[query_0_int]]
// CHECK-NEXT: [[query2_1:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 1
// CHECK-NEXT:  [[query_1_int:%[0-9]+]] = OpBitcast %int [[query2_1]]
// CHECK-NEXT:                      OpStore %i_height [[query_1_int]]
// CHECK-NEXT:   [[query_level_2:%[0-9]+]] = OpImageQueryLevels %uint [[image2]]
// CHECK-NEXT:  [[query_level_2_int:%[0-9]+]] = OpBitcast %int [[query_level_2]]
// CHECK-NEXT:                      OpStore %i_numLevels [[query_level_2_int]]
  tex2d.GetDimensions(mipLevel, i_width, i_height, i_numLevels);

#ifdef ERROR
// ERROR: error: Output argument must be an l-value
  tex2d.GetDimensions(mipLevel, 0, height, numLevels);

// ERROR: error: Output argument must be an l-value
  tex2d.GetDimensions(width, 20);
#endif
}