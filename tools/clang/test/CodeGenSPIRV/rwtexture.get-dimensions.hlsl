// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ImageQuery

RWTexture1D        <float> t1;
RWTexture1DArray   <float> t2;
RWTexture2D        <float> t3;
RWTexture2DArray   <float> t4;
RWTexture3D        <float> t5;

void main() {
  uint width, height, depth, elements;

// CHECK:            [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[query1:%\d+]] = OpImageQuerySize %uint [[t1]]
// CHECK-NEXT:                     OpStore %width [[query1]]
  t1.GetDimensions(width);

// CHECK:            [[t2:%\d+]] = OpLoad %type_1d_image_array %t2
// CHECK-NEXT:   [[query2:%\d+]] = OpImageQuerySize %v2uint [[t2]]
// CHECK-NEXT: [[query2_0:%\d+]] = OpCompositeExtract %uint [[query2]] 0
// CHECK-NEXT:                     OpStore %width [[query2_0]]
// CHECK-NEXT: [[query2_1:%\d+]] = OpCompositeExtract %uint [[query2]] 1
// CHECK-NEXT:                     OpStore %elements [[query2_1]]  
  t2.GetDimensions(width, elements);

// CHECK:            [[t3:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:   [[query3:%\d+]] = OpImageQuerySize %v2uint [[t3]]
// CHECK-NEXT: [[query3_0:%\d+]] = OpCompositeExtract %uint [[query3]] 0
// CHECK-NEXT:                     OpStore %width [[query3_0]]
// CHECK-NEXT: [[query3_1:%\d+]] = OpCompositeExtract %uint [[query3]] 1
// CHECK-NEXT:                     OpStore %height [[query3_1]]  
  t3.GetDimensions(width, height);

// CHECK:            [[t4:%\d+]] = OpLoad %type_2d_image_array %t4
// CHECK-NEXT:   [[query4:%\d+]] = OpImageQuerySize %v3uint [[t4]]
// CHECK-NEXT: [[query4_0:%\d+]] = OpCompositeExtract %uint [[query4]] 0
// CHECK-NEXT:                     OpStore %width [[query4_0]]
// CHECK-NEXT: [[query4_1:%\d+]] = OpCompositeExtract %uint [[query4]] 1
// CHECK-NEXT:                     OpStore %height [[query4_1]]
// CHECK-NEXT: [[query4_2:%\d+]] = OpCompositeExtract %uint [[query4]] 2
// CHECK-NEXT:                     OpStore %elements [[query4_2]]
  t4.GetDimensions(width, height, elements);

// CHECK:            [[t5:%\d+]] = OpLoad %type_3d_image %t5
// CHECK-NEXT:   [[query5:%\d+]] = OpImageQuerySize %v3uint [[t5]]
// CHECK-NEXT: [[query5_0:%\d+]] = OpCompositeExtract %uint [[query5]] 0
// CHECK-NEXT:                     OpStore %width [[query5_0]]
// CHECK-NEXT: [[query5_1:%\d+]] = OpCompositeExtract %uint [[query5]] 1
// CHECK-NEXT:                     OpStore %height [[query5_1]]
// CHECK-NEXT: [[query5_2:%\d+]] = OpCompositeExtract %uint [[query5]] 2
// CHECK-NEXT:                     OpStore %depth [[query5_2]]
  t5.GetDimensions(width, height, depth);
}
