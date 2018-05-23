// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ImageQuery

Texture1D        <float> t1;
Texture1DArray   <float> t2;
Texture2D        <float> t3;
Texture2DArray   <float> t4;
Texture3D        <float> t5;
Texture2DMS      <float> t6;
Texture2DMSArray <float> t7;
TextureCube      <float> t8;
TextureCubeArray <float> t9;

void main() {
  uint mipLevel = 1;
  uint width, height, depth, elements, numLevels, numSamples;

// CHECK:        [[t1_0:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT: [[query0:%\d+]] = OpImageQuerySizeLod %uint [[t1_0]] %int_0
// CHECK-NEXT:                   OpStore %width [[query0]]
  t1.GetDimensions(width);

// CHECK:        [[t1_1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[mip0:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT: [[query1:%\d+]] = OpImageQuerySizeLod %uint [[t1_1]] [[mip0]]
// CHECK-NEXT:                   OpStore %width [[query1]]
// CHECK-NEXT: [[query2:%\d+]] = OpImageQueryLevels %uint [[t1_1]]
// CHECK-NEXT:                   OpStore %numLevels [[query2]]
  t1.GetDimensions(mipLevel, width, numLevels);

// CHECK:          [[t2_0:%\d+]] = OpLoad %type_1d_image_array %t2
// CHECK-NEXT:   [[query3:%\d+]] = OpImageQuerySizeLod %v2uint [[t2_0]] %int_0
// CHECK-NEXT: [[query3_0:%\d+]] = OpCompositeExtract %uint [[query3]] 0
// CHECK-NEXT:                     OpStore %width [[query3_0]]
// CHECK-NEXT: [[query3_1:%\d+]] = OpCompositeExtract %uint [[query3]] 1
// CHECK-NEXT:                     OpStore %elements [[query3_1]]
  t2.GetDimensions(width, elements);

// CHECK:          [[t2_1:%\d+]] = OpLoad %type_1d_image_array %t2
// CHECK-NEXT:     [[mip1:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query4:%\d+]] = OpImageQuerySizeLod %v2uint [[t2_1]] [[mip1]]
// CHECK-NEXT: [[query4_0:%\d+]] = OpCompositeExtract %uint [[query4]] 0
// CHECK-NEXT:                     OpStore %width [[query4_0]]
// CHECK-NEXT: [[query4_1:%\d+]] = OpCompositeExtract %uint [[query4]] 1
// CHECK-NEXT:                     OpStore %elements [[query4_1]]
// CHECK-NEXT:   [[query5:%\d+]] = OpImageQueryLevels %uint [[t2_1]]
// CHECK-NEXT:                     OpStore %numLevels [[query5]]
  t2.GetDimensions(mipLevel, width, elements, numLevels);

// CHECK:          [[t3_0:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:   [[query5:%\d+]] = OpImageQuerySizeLod %v2uint [[t3_0]] %int_0
// CHECK-NEXT: [[query5_0:%\d+]] = OpCompositeExtract %uint [[query5]] 0
// CHECK-NEXT:                     OpStore %width [[query5_0]]
// CHECK-NEXT: [[query5_1:%\d+]] = OpCompositeExtract %uint [[query5]] 1
// CHECK-NEXT:                     OpStore %height [[query5_1]]
  t3.GetDimensions(width, height);
  
// CHECK:          [[t3_1:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:     [[mip2:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query6:%\d+]] = OpImageQuerySizeLod %v2uint [[t3_1]] [[mip2]]
// CHECK-NEXT: [[query6_0:%\d+]] = OpCompositeExtract %uint [[query6]] 0
// CHECK-NEXT:                     OpStore %width [[query6_0]]
// CHECK-NEXT: [[query6_1:%\d+]] = OpCompositeExtract %uint [[query6]] 1
// CHECK-NEXT:                     OpStore %height [[query6_1]]
// CHECK-NEXT:   [[query7:%\d+]] = OpImageQueryLevels %uint [[t3_1]]
// CHECK-NEXT:                     OpStore %numLevels [[query7]]
  t3.GetDimensions(mipLevel, width, height, numLevels);

// CHECK:          [[t4_0:%\d+]] = OpLoad %type_2d_image_array %t4
// CHECK-NEXT:   [[query8:%\d+]] = OpImageQuerySizeLod %v3uint [[t4_0]] %int_0
// CHECK-NEXT: [[query8_0:%\d+]] = OpCompositeExtract %uint [[query8]] 0
// CHECK-NEXT:                     OpStore %width [[query8_0]]
// CHECK-NEXT: [[query8_1:%\d+]] = OpCompositeExtract %uint [[query8]] 1
// CHECK-NEXT:                     OpStore %height [[query8_1]]
// CHECK-NEXT: [[query8_2:%\d+]] = OpCompositeExtract %uint [[query8]] 2
// CHECK-NEXT:                     OpStore %elements [[query8_2]]
  t4.GetDimensions(width, height, elements);
  
// CHECK:          [[t4_1:%\d+]] = OpLoad %type_2d_image_array %t4
// CHECK-NEXT:     [[mip3:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query9:%\d+]] = OpImageQuerySizeLod %v3uint [[t4_1]] [[mip3]]
// CHECK-NEXT: [[query9_0:%\d+]] = OpCompositeExtract %uint [[query9]] 0
// CHECK-NEXT:                     OpStore %width [[query9_0]]
// CHECK-NEXT: [[query9_1:%\d+]] = OpCompositeExtract %uint [[query9]] 1
// CHECK-NEXT:                     OpStore %height [[query9_1]]
// CHECK-NEXT: [[query9_2:%\d+]] = OpCompositeExtract %uint [[query9]] 2
// CHECK-NEXT:                     OpStore %elements [[query9_2]]
// CHECK-NEXT:  [[query10:%\d+]] = OpImageQueryLevels %uint [[t4_1]]
// CHECK-NEXT:                     OpStore %numLevels [[query10]]
  t4.GetDimensions(mipLevel, width, height, elements, numLevels);

// CHECK:           [[t5_0:%\d+]] = OpLoad %type_3d_image %t5
// CHECK-NEXT:   [[query11:%\d+]] = OpImageQuerySizeLod %v3uint [[t5_0]] %int_0
// CHECK-NEXT: [[query11_0:%\d+]] = OpCompositeExtract %uint [[query11]] 0
// CHECK-NEXT:                      OpStore %width [[query11_0]]
// CHECK-NEXT: [[query11_1:%\d+]] = OpCompositeExtract %uint [[query11]] 1
// CHECK-NEXT:                      OpStore %height [[query11_1]]
// CHECK-NEXT: [[query11_2:%\d+]] = OpCompositeExtract %uint [[query11]] 2
// CHECK-NEXT:                      OpStore %depth [[query11_2]]
  t5.GetDimensions(width, height, depth);

// CHECK:           [[t5_1:%\d+]] = OpLoad %type_3d_image %t5
// CHECK-NEXT:      [[mip4:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query12:%\d+]] = OpImageQuerySizeLod %v3uint [[t5_1]] [[mip4]]
// CHECK-NEXT: [[query12_0:%\d+]] = OpCompositeExtract %uint [[query12]] 0
// CHECK-NEXT:                      OpStore %width [[query12_0]]
// CHECK-NEXT: [[query12_1:%\d+]] = OpCompositeExtract %uint [[query12]] 1
// CHECK-NEXT:                      OpStore %height [[query12_1]]
// CHECK-NEXT: [[query12_2:%\d+]] = OpCompositeExtract %uint [[query12]] 2
// CHECK-NEXT:                      OpStore %depth [[query12_2]]
// CHECK-NEXT:   [[query13:%\d+]] = OpImageQueryLevels %uint [[t5_1]]
// CHECK-NEXT:                      OpStore %numLevels [[query13]]
  t5.GetDimensions(mipLevel, width, height, depth, numLevels);

// CHECK:             [[t6:%\d+]] = OpLoad %type_2d_image_0 %t6
// CHECK-NEXT:   [[query14:%\d+]] = OpImageQuerySize %v2uint [[t6]]
// CHECK-NEXT: [[query14_0:%\d+]] = OpCompositeExtract %uint [[query14]] 0
// CHECK-NEXT:                      OpStore %width [[query14_0]]
// CHECK-NEXT: [[query14_1:%\d+]] = OpCompositeExtract %uint [[query14]] 1
// CHECK-NEXT:                      OpStore %height [[query14_1]]
// CHECK-NEXT:   [[query15:%\d+]] = OpImageQuerySamples %uint [[t6]]
// CHECK-NEXT:                      OpStore %numSamples [[query15]]
  t6.GetDimensions(width, height, numSamples);

// CHECK:             [[t7:%\d+]] = OpLoad %type_2d_image_array_0 %t7
// CHECK-NEXT:   [[query16:%\d+]] = OpImageQuerySize %v3uint [[t7]]
// CHECK-NEXT: [[query16_0:%\d+]] = OpCompositeExtract %uint [[query16]] 0
// CHECK-NEXT:                      OpStore %width [[query16_0]]
// CHECK-NEXT: [[query16_1:%\d+]] = OpCompositeExtract %uint [[query16]] 1
// CHECK-NEXT:                      OpStore %height [[query16_1]]
// CHECK-NEXT: [[query16_2:%\d+]] = OpCompositeExtract %uint [[query16]] 2
// CHECK-NEXT:                      OpStore %elements [[query16_2]]
// CHECK-NEXT:   [[query17:%\d+]] = OpImageQuerySamples %uint [[t7]]
// CHECK-NEXT:                      OpStore %numSamples [[query17]]
  t7.GetDimensions(width, height, elements, numSamples);

  // Overloads with float output arg.
  float f_width, f_height, f_depth, f_elements, f_numSamples, f_numLevels;

// CHECK:          [[t1_0:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[query0:%\d+]] = OpImageQuerySizeLod %uint [[t1_0]] %int_0
// CHECK-NEXT: [[f_query0:%\d+]] = OpConvertUToF %float [[query0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query0]]
  t1.GetDimensions(f_width);

// CHECK:        [[t1_1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[mip0:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT: [[query1:%\d+]] = OpImageQuerySizeLod %uint [[t1_1]] [[mip0]]
// CHECK-NEXT: [[f_query1:%\d+]] = OpConvertUToF %float [[query1]]
// CHECK-NEXT:                   OpStore %f_width [[f_query1]]
// CHECK-NEXT: [[query2:%\d+]] = OpImageQueryLevels %uint [[t1_1]]
// CHECK-NEXT: [[f_query2:%\d+]] = OpConvertUToF %float [[query2]]
// CHECK-NEXT:                   OpStore %f_numLevels [[f_query2]]
  t1.GetDimensions(mipLevel, f_width, f_numLevels);

// CHECK:          [[t2_0:%\d+]] = OpLoad %type_1d_image_array %t2
// CHECK-NEXT:   [[query3:%\d+]] = OpImageQuerySizeLod %v2uint [[t2_0]] %int_0
// CHECK-NEXT: [[query3_0:%\d+]] = OpCompositeExtract %uint [[query3]] 0
// CHECK-NEXT: [[f_query3_0:%\d+]] = OpConvertUToF %float [[query3_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query3_0]]
// CHECK-NEXT: [[query3_1:%\d+]] = OpCompositeExtract %uint [[query3]] 1
// CHECK-NEXT: [[f_query3_1:%\d+]] = OpConvertUToF %float [[query3_1]]
// CHECK-NEXT:                     OpStore %f_elements [[f_query3_1]]
  t2.GetDimensions(f_width, f_elements);

// CHECK:          [[t2_1:%\d+]] = OpLoad %type_1d_image_array %t2
// CHECK-NEXT:     [[mip1:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query4:%\d+]] = OpImageQuerySizeLod %v2uint [[t2_1]] [[mip1]]
// CHECK-NEXT: [[query4_0:%\d+]] = OpCompositeExtract %uint [[query4]] 0
// CHECK-NEXT: [[f_query4_0:%\d+]] = OpConvertUToF %float [[query4_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query4_0]]
// CHECK-NEXT: [[query4_1:%\d+]] = OpCompositeExtract %uint [[query4]] 1
// CHECK-NEXT: [[f_query4_1:%\d+]] = OpConvertUToF %float [[query4_1]]
// CHECK-NEXT:                     OpStore %f_elements [[f_query4_1]]
// CHECK-NEXT:   [[query5:%\d+]] = OpImageQueryLevels %uint [[t2_1]]
// CHECK-NEXT: [[f_query5:%\d+]] = OpConvertUToF %float [[query5]]
// CHECK-NEXT:                     OpStore %f_numLevels [[f_query5]]
  t2.GetDimensions(mipLevel, f_width, f_elements, f_numLevels);

// CHECK:          [[t3_0:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:   [[query5:%\d+]] = OpImageQuerySizeLod %v2uint [[t3_0]] %int_0
// CHECK-NEXT: [[query5_0:%\d+]] = OpCompositeExtract %uint [[query5]] 0
// CHECK-NEXT: [[f_query5_0:%\d+]] = OpConvertUToF %float [[query5_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query5_0]]
// CHECK-NEXT: [[query5_1:%\d+]] = OpCompositeExtract %uint [[query5]] 1
// CHECK-NEXT: [[f_query5_1:%\d+]] = OpConvertUToF %float [[query5_1]]
// CHECK-NEXT:                     OpStore %f_height [[f_query5_1]]
  t3.GetDimensions(f_width, f_height);
  
// CHECK:          [[t3_1:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:     [[mip2:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query6:%\d+]] = OpImageQuerySizeLod %v2uint [[t3_1]] [[mip2]]
// CHECK-NEXT: [[query6_0:%\d+]] = OpCompositeExtract %uint [[query6]] 0
// CHECK-NEXT: [[f_query6_0:%\d+]] = OpConvertUToF %float [[query6_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query6_0]]
// CHECK-NEXT: [[query6_1:%\d+]] = OpCompositeExtract %uint [[query6]] 1
// CHECK-NEXT: [[f_query6_1:%\d+]] = OpConvertUToF %float [[query6_1]]
// CHECK-NEXT:                     OpStore %f_height [[f_query6_1]]
// CHECK-NEXT:   [[query7:%\d+]] = OpImageQueryLevels %uint [[t3_1]]
// CHECK-NEXT: [[f_query7:%\d+]] = OpConvertUToF %float [[query7]]
// CHECK-NEXT:                     OpStore %f_numLevels [[f_query7]]
  t3.GetDimensions(mipLevel, f_width, f_height, f_numLevels);

// CHECK:          [[t4_0:%\d+]] = OpLoad %type_2d_image_array %t4
// CHECK-NEXT:   [[query8:%\d+]] = OpImageQuerySizeLod %v3uint [[t4_0]] %int_0
// CHECK-NEXT: [[query8_0:%\d+]] = OpCompositeExtract %uint [[query8]] 0
// CHECK-NEXT: [[f_query8_0:%\d+]] = OpConvertUToF %float [[query8_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query8_0]]
// CHECK-NEXT: [[query8_1:%\d+]] = OpCompositeExtract %uint [[query8]] 1
// CHECK-NEXT: [[f_query8_1:%\d+]] = OpConvertUToF %float [[query8_1]]
// CHECK-NEXT:                     OpStore %f_height [[f_query8_1]]
// CHECK-NEXT: [[query8_2:%\d+]] = OpCompositeExtract %uint [[query8]] 2
// CHECK-NEXT: [[f_query8_2:%\d+]] = OpConvertUToF %float [[query8_2]]
// CHECK-NEXT:                     OpStore %f_elements [[f_query8_2]]
  t4.GetDimensions(f_width, f_height, f_elements);
  
// CHECK:          [[t4_1:%\d+]] = OpLoad %type_2d_image_array %t4
// CHECK-NEXT:     [[mip3:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query9:%\d+]] = OpImageQuerySizeLod %v3uint [[t4_1]] [[mip3]]
// CHECK-NEXT: [[query9_0:%\d+]] = OpCompositeExtract %uint [[query9]] 0
// CHECK-NEXT: [[f_query9_0:%\d+]] = OpConvertUToF %float [[query9_0]]
// CHECK-NEXT:                     OpStore %f_width [[f_query9_0]]
// CHECK-NEXT: [[query9_1:%\d+]] = OpCompositeExtract %uint [[query9]] 1
// CHECK-NEXT: [[f_query9_1:%\d+]] = OpConvertUToF %float [[query9_1]]
// CHECK-NEXT:                     OpStore %f_height [[f_query9_1]]
// CHECK-NEXT: [[query9_2:%\d+]] = OpCompositeExtract %uint [[query9]] 2
// CHECK-NEXT: [[f_query9_2:%\d+]] = OpConvertUToF %float [[query9_2]]
// CHECK-NEXT:                     OpStore %f_elements [[f_query9_2]]
// CHECK-NEXT:  [[query10:%\d+]] = OpImageQueryLevels %uint [[t4_1]]
// CHECK-NEXT: [[f_query10:%\d+]] = OpConvertUToF %float [[query10]]
// CHECK-NEXT:                     OpStore %f_numLevels [[f_query10]]
  t4.GetDimensions(mipLevel, f_width, f_height, f_elements, f_numLevels);

// CHECK:           [[t5_0:%\d+]] = OpLoad %type_3d_image %t5
// CHECK-NEXT:   [[query11:%\d+]] = OpImageQuerySizeLod %v3uint [[t5_0]] %int_0
// CHECK-NEXT: [[query11_0:%\d+]] = OpCompositeExtract %uint [[query11]] 0
// CHECK-NEXT: [[f_query11_0:%\d+]] = OpConvertUToF %float [[query11_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query11_0]]
// CHECK-NEXT: [[query11_1:%\d+]] = OpCompositeExtract %uint [[query11]] 1
// CHECK-NEXT: [[f_query11_1:%\d+]] = OpConvertUToF %float [[query11_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query11_1]]
// CHECK-NEXT: [[query11_2:%\d+]] = OpCompositeExtract %uint [[query11]] 2
// CHECK-NEXT: [[f_query11_2:%\d+]] = OpConvertUToF %float [[query11_2]]
// CHECK-NEXT:                      OpStore %f_depth [[f_query11_2]]
  t5.GetDimensions(f_width, f_height, f_depth);

// CHECK:           [[t5_1:%\d+]] = OpLoad %type_3d_image %t5
// CHECK-NEXT:      [[mip4:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query12:%\d+]] = OpImageQuerySizeLod %v3uint [[t5_1]] [[mip4]]
// CHECK-NEXT: [[query12_0:%\d+]] = OpCompositeExtract %uint [[query12]] 0
// CHECK-NEXT: [[f_query12_0:%\d+]] = OpConvertUToF %float [[query12_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query12_0]]
// CHECK-NEXT: [[query12_1:%\d+]] = OpCompositeExtract %uint [[query12]] 1
// CHECK-NEXT: [[f_query12_1:%\d+]] = OpConvertUToF %float [[query12_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query12_1]]
// CHECK-NEXT: [[query12_2:%\d+]] = OpCompositeExtract %uint [[query12]] 2
// CHECK-NEXT: [[f_query12_2:%\d+]] = OpConvertUToF %float [[query12_2]]
// CHECK-NEXT:                      OpStore %f_depth [[f_query12_2]]
// CHECK-NEXT:   [[query13:%\d+]] = OpImageQueryLevels %uint [[t5_1]]
// CHECK-NEXT: [[f_query13:%\d+]] = OpConvertUToF %float [[query13]]
// CHECK-NEXT:                      OpStore %f_numLevels [[f_query13]]
  t5.GetDimensions(mipLevel, f_width, f_height, f_depth, f_numLevels);

// CHECK:             [[t6:%\d+]] = OpLoad %type_2d_image_0 %t6
// CHECK-NEXT:   [[query14:%\d+]] = OpImageQuerySize %v2uint [[t6]]
// CHECK-NEXT: [[query14_0:%\d+]] = OpCompositeExtract %uint [[query14]] 0
// CHECK-NEXT: [[f_query14_0:%\d+]] = OpConvertUToF %float [[query14_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query14_0]]
// CHECK-NEXT: [[query14_1:%\d+]] = OpCompositeExtract %uint [[query14]] 1
// CHECK-NEXT: [[f_query14_1:%\d+]] = OpConvertUToF %float [[query14_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query14_1]]
// CHECK-NEXT:   [[query15:%\d+]] = OpImageQuerySamples %uint [[t6]]
// CHECK-NEXT: [[f_query15:%\d+]] = OpConvertUToF %float [[query15]]
// CHECK-NEXT:                      OpStore %f_numSamples [[f_query15]]
  t6.GetDimensions(f_width, f_height, f_numSamples);

// CHECK:             [[t7:%\d+]] = OpLoad %type_2d_image_array_0 %t7
// CHECK-NEXT:   [[query16:%\d+]] = OpImageQuerySize %v3uint [[t7]]
// CHECK-NEXT: [[query16_0:%\d+]] = OpCompositeExtract %uint [[query16]] 0
// CHECK-NEXT: [[f_query16_0:%\d+]] = OpConvertUToF %float [[query16_0]]
// CHECK-NEXT:                      OpStore %f_width [[f_query16_0]]
// CHECK-NEXT: [[query16_1:%\d+]] = OpCompositeExtract %uint [[query16]] 1
// CHECK-NEXT: [[f_query16_1:%\d+]] = OpConvertUToF %float [[query16_1]]
// CHECK-NEXT:                      OpStore %f_height [[f_query16_1]]
// CHECK-NEXT: [[query16_2:%\d+]] = OpCompositeExtract %uint [[query16]] 2
// CHECK-NEXT: [[f_query16_2:%\d+]] = OpConvertUToF %float [[query16_2]]
// CHECK-NEXT:                      OpStore %f_elements [[f_query16_2]]
// CHECK-NEXT:   [[query17:%\d+]] = OpImageQuerySamples %uint [[t7]]
// CHECK-NEXT: [[f_query17:%\d+]] = OpConvertUToF %float [[query17]]
// CHECK-NEXT:                      OpStore %f_numSamples [[f_query17]]
  t7.GetDimensions(f_width, f_height, f_elements, f_numSamples);

// CHECK:             [[t8:%\d+]] = OpLoad %type_cube_image %t8
// CHECK-NEXT:   [[query18:%\d+]] = OpImageQuerySizeLod %v2uint [[t8]] %int_0
// CHECK-NEXT: [[query18_0:%\d+]] = OpCompositeExtract %uint [[query18]] 0
// CHECK-NEXT:                      OpStore %width [[query18_0]]
// CHECK-NEXT: [[query18_1:%\d+]] = OpCompositeExtract %uint [[query18]] 1
// CHECK-NEXT:                      OpStore %height [[query18_1]]
  t8.GetDimensions(width, height);
  
// CHECK:             [[t8:%\d+]] = OpLoad %type_cube_image %t8
// CHECK-NEXT:       [[mip:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query19:%\d+]] = OpImageQuerySizeLod %v2uint [[t8]] [[mip]]
// CHECK-NEXT: [[query19_0:%\d+]] = OpCompositeExtract %uint [[query19]] 0
// CHECK-NEXT:                      OpStore %width [[query19_0]]
// CHECK-NEXT: [[query19_1:%\d+]] = OpCompositeExtract %uint [[query19]] 1
// CHECK-NEXT:                      OpStore %height [[query19_1]]
// CHECK-NEXT:   [[query20:%\d+]] = OpImageQueryLevels %uint [[t8]]
// CHECK-NEXT:                      OpStore %numLevels [[query20]]
  t8.GetDimensions(mipLevel, width, height, numLevels);

// CHECK:             [[t9:%\d+]] = OpLoad %type_cube_image_array %t9
// CHECK-NEXT:   [[query21:%\d+]] = OpImageQuerySizeLod %v3uint [[t9]] %int_0
// CHECK-NEXT: [[query21_0:%\d+]] = OpCompositeExtract %uint [[query21]] 0
// CHECK-NEXT:                      OpStore %width [[query21_0]]
// CHECK-NEXT: [[query21_1:%\d+]] = OpCompositeExtract %uint [[query21]] 1
// CHECK-NEXT:                      OpStore %height [[query21_1]]
// CHECK-NEXT: [[query21_2:%\d+]] = OpCompositeExtract %uint [[query21]] 2
// CHECK-NEXT:                      OpStore %elements [[query21_2]]
  t9.GetDimensions(width, height, elements);
  
// CHECK:             [[t9:%\d+]] = OpLoad %type_cube_image_array %t9
// CHECK-NEXT:       [[mip:%\d+]] = OpLoad %uint %mipLevel
// CHECK-NEXT:   [[query22:%\d+]] = OpImageQuerySizeLod %v3uint [[t9]] [[mip]]
// CHECK-NEXT: [[query22_0:%\d+]] = OpCompositeExtract %uint [[query22]] 0
// CHECK-NEXT:                      OpStore %width [[query22_0]]
// CHECK-NEXT: [[query22_1:%\d+]] = OpCompositeExtract %uint [[query22]] 1
// CHECK-NEXT:                      OpStore %height [[query22_1]]
// CHECK-NEXT: [[query22_2:%\d+]] = OpCompositeExtract %uint [[query22]] 2
// CHECK-NEXT:                      OpStore %elements [[query22_2]]
// CHECK-NEXT:   [[query23:%\d+]] = OpImageQueryLevels %uint [[t9]]
// CHECK-NEXT:                      OpStore %numLevels [[query23]]
  t9.GetDimensions(mipLevel, width, height, elements, numLevels);

// Try with signed integer as argument.

// CHECK:                [[t3:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:        [[query:%\d+]] = OpImageQuerySizeLod %v2uint [[t3]] %int_0
// CHECK-NEXT: [[query_0_uint:%\d+]] = OpCompositeExtract %uint [[query]] 0
// CHECK-NEXT:  [[query_0_int:%\d+]] = OpBitcast %int [[query_0_uint]]
// CHECK-NEXT:                         OpStore %signedWidth [[query_0_int]]
// CHECK-NEXT: [[query_1_uint:%\d+]] = OpCompositeExtract %uint [[query]] 1
// CHECK-NEXT:  [[query_1_int:%\d+]] = OpBitcast %int [[query_1_uint]]
// CHECK-NEXT:                         OpStore %signedHeight [[query_1_int]]
  int signedMipLevel, signedWidth, signedHeight, signedNumLevels;
  t3.GetDimensions(signedWidth, signedHeight);

// CHECK-NEXT:                [[t3:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:    [[signedMipLevel:%\d+]] = OpLoad %int %signedMipLevel
// CHECK-NEXT:  [[unsignedMipLevel:%\d+]] = OpBitcast %uint [[signedMipLevel]]
// CHECK-NEXT:             [[query:%\d+]] = OpImageQuerySizeLod %v2uint [[t3]] [[unsignedMipLevel]]
// CHECK-NEXT:      [[query_0_uint:%\d+]] = OpCompositeExtract %uint [[query]] 0
// CHECK-NEXT:       [[query_0_int:%\d+]] = OpBitcast %int [[query_0_uint]]
// CHECK-NEXT:                              OpStore %signedWidth [[query_0_int]]
// CHECK-NEXT:      [[query_1_uint:%\d+]] = OpCompositeExtract %uint [[query]] 1
// CHECK-NEXT:       [[query_1_int:%\d+]] = OpBitcast %int [[query_1_uint]]
// CHECK-NEXT:                              OpStore %signedHeight [[query_1_int]]
// CHECK-NEXT: [[query_levels_uint:%\d+]] = OpImageQueryLevels %uint [[t3]]
// CHECK-NEXT:  [[query_levels_int:%\d+]] = OpBitcast %int [[query_levels_uint]]
// CHECK-NEXT:                              OpStore %signedNumLevels [[query_levels_int]]
  t3.GetDimensions(signedMipLevel, signedWidth, signedHeight, signedNumLevels);
}
