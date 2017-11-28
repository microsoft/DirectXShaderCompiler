// Run: %dxc -T ps_6_0 -E main

Texture1D        <float>  t1;
Texture2D        <int2>   t2;
Texture3D        <uint3>  t3;
Texture2DMS      <float4> t4;
Texture1DArray   <float4> t5;
Texture2DArray   <int3>   t6;
// There is no operator[] for TextureCube      in HLSL reference.
// There is no operator[] for TextureCubeArray in HLSL reference.
// There is no operator[] for Texture2DMSArray in HLSL reference.


// CHECK:  [[cu12:%\d+]] = OpConstantComposite %v2uint %uint_1 %uint_2
// CHECK: [[cu123:%\d+]] = OpConstantComposite %v3uint %uint_1 %uint_2 %uint_3

void main() {

// CHECK:           [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:      [[f1:%\d+]] = OpImageFetch %v4float [[t1]] %uint_5 Lod %uint_0
// CHECK-NEXT: [[result1:%\d+]] = OpCompositeExtract %float [[f1]] 0
// CHECK-NEXT:                    OpStore %a1 [[result1]]
  float  a1 = t1[5];

// CHECK-NEXT:                    OpStore %pos2 [[cu12]]
// CHECK-NEXT:    [[pos2:%\d+]] = OpLoad %v2uint %pos2
// CHECK-NEXT:      [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:      [[f2:%\d+]] = OpImageFetch %v4int [[t2]] [[pos2]] Lod %uint_0
// CHECK-NEXT: [[result2:%\d+]] = OpVectorShuffle %v2int [[f2]] [[f2]] 0 1
// CHECK-NEXT:                    OpStore %a2 [[result2]]
  uint2 pos2 = uint2(1,2);
  int2    a2 = t2[pos2];

// CHECK-NEXT:                    OpStore %pos3 [[cu123]]
// CHECK-NEXT:    [[pos3:%\d+]] = OpLoad %v3uint %pos3
// CHECK-NEXT:      [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:      [[f3:%\d+]] = OpImageFetch %v4uint [[t3]] [[pos3]] Lod %uint_0
// CHECK-NEXT: [[result3:%\d+]] = OpVectorShuffle %v3uint [[f3]] [[f3]] 0 1 2
// CHECK-NEXT:                    OpStore %a3 [[result3]]
  uint3 pos3 = uint3(1,2,3);
  uint3   a3 = t3[pos3];

// CHECK-NEXT:      [[t4:%\d+]] = OpLoad %type_2d_image_0 %t4
// CHECK-NEXT:      [[f4:%\d+]] = OpImageFetch %v4float [[t4]] [[cu12]] Sample %uint_0
// CHECK-NEXT:                    OpStore %a4 [[f4]]
  float4 a4 = t4[uint2(1,2)];

// CHECK-NEXT:      [[t5:%\d+]] = OpLoad %type_1d_image_array %t5
// CHECK-NEXT:      [[f5:%\d+]] = OpImageFetch %v4float [[t5]] [[cu12]] Lod %uint_0
// CHECK-NEXT:                    OpStore %a5 [[f5]]
  float4 a5 = t5[uint2(1,2)];

// CHECK-NEXT:      [[t6:%\d+]] = OpLoad %type_2d_image_array %t6
// CHECK-NEXT:      [[f6:%\d+]] = OpImageFetch %v4int [[t6]] [[cu123]] Lod %uint_0
// CHECK-NEXT: [[result6:%\d+]] = OpVectorShuffle %v3int [[f6]] [[f6]] 0 1 2
// CHECK-NEXT: OpStore %a6 [[result6]]
  int3   a6 = t6[uint3(1,2,3)];
}
