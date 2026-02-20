// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s
// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv -DERROR 2>&1 | FileCheck %s --check-prefix=ERROR

// CHECK: OpCapability ImageQuery

vk::SampledTexture2D<float4> t1;

void main() {
  uint mipLevel = 1;
  uint width, height, numLevels;

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT:                      OpStore %width [[query1_0]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT:                      OpStore %height [[query1_1]]
  t1.GetDimensions(width, height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
// CHECK-NEXT:       [[mip:%[0-9]+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query2:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image2]] [[mip]]
// CHECK-NEXT: [[query2_0:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 0
// CHECK-NEXT:                      OpStore %width [[query2_0]]
// CHECK-NEXT: [[query2_1:%[0-9]+]] = OpCompositeExtract %uint [[query2]] 1
// CHECK-NEXT:                      OpStore %height [[query2_1]]
// CHECK-NEXT:   [[query_level_2:%[0-9]+]] = OpImageQueryLevels %uint [[image2]]
// CHECK-NEXT:                      OpStore %numLevels [[query_level_2]]
  t1.GetDimensions(mipLevel, width, height, numLevels);

  float f_width, f_height, f_numLevels;
// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT: [[f_query1_0:%[0-9]+]] = OpConvertUToF %float [[query1_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query1_0]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT: [[f_query1_1:%[0-9]+]] = OpConvertUToF %float [[query1_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query1_1]]
  t1.GetDimensions(f_width, f_height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
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
  t1.GetDimensions(mipLevel, f_width, f_height, f_numLevels);

  int i_width, i_height, i_numLevels;
// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image1:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
// CHECK-NEXT:   [[query1:%[0-9]+]] = OpImageQuerySizeLod %v2uint [[image1]] %int_0
// CHECK-NEXT: [[query1_0:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 0
// CHECK-NEXT:  [[query_0_int:%[0-9]+]] = OpBitcast %int [[query1_0]]
// CHECK-NEXT:                      OpStore %i_width [[query_0_int]]
// CHECK-NEXT: [[query1_1:%[0-9]+]] = OpCompositeExtract %uint [[query1]] 1
// CHECK-NEXT:  [[query_1_int:%[0-9]+]] = OpBitcast %int [[query1_1]]
// CHECK-NEXT:                      OpStore %i_height [[query_1_int]]
  t1.GetDimensions(i_width, i_height);

// CHECK:             [[t1_load:%[0-9]+]] = OpLoad %type_sampled_image %t1
// CHECK-NEXT:   [[image2:%[0-9]+]] = OpImage %type_2d_image [[t1_load]]
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
  t1.GetDimensions(mipLevel, i_width, i_height, i_numLevels);

#ifdef ERROR
// ERROR: error: Output argument must be an l-value
  t1.GetDimensions(mipLevel, 0, height, numLevels);

// ERROR: error: Output argument must be an l-value
  t1.GetDimensions(width, 20);
#endif
}