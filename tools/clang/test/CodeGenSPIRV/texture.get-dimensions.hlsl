// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ImageQuery

Texture1D        <float> t1;
Texture1DArray   <float> t2;
Texture2D        <float> t3;
Texture2DArray   <float> t4;
Texture3D        <float> t5;
Texture2DMS      <float> t6;
Texture2DMSArray <float> t7;

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
}
