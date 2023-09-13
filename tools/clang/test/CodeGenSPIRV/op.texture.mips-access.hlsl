// RUN: %dxc -T ps_6_0 -E main

Texture1D        <float>  t1;
Texture2D        <int2>   t2;
Texture3D        <uint3>  t3;
Texture1DArray   <float4> t4;
Texture2DArray   <int3>   t5;

// There is no .mips[][] for TextureCube      in HLSL reference.
// There is no .mips[][] for TextureCubeArray in HLSL reference.

// CHECK:  [[cu12:%\d+]] = OpConstantComposite %v2uint %uint_1 %uint_2
// CHECK: [[cu123:%\d+]] = OpConstantComposite %v3uint %uint_1 %uint_2 %uint_3

void main() {

  uint pos = 7;
  uint2 pos2 = uint2(1,2);
  uint3 pos3 = uint3(1,2,3);

// CHECK:        [[pos:%\d+]] = OpLoad %uint %pos
// CHECK-NEXT:    [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:    [[f1:%\d+]] = OpImageFetch %v4float [[t1]] [[pos]] Lod %uint_1
// CHECK-NEXT:  [[val1:%\d+]] = OpCompositeExtract %float [[f1]] 0
// CHECK-NEXT:                  OpStore %a1 [[val1]]
  float  a1 = t1.mips[1][pos];

// CHECK-NEXT:  [[pos2:%\d+]] = OpLoad %v2uint %pos2
// CHECK-NEXT:    [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:    [[f2:%\d+]] = OpImageFetch %v4int [[t2]] [[pos2]] Lod %uint_2
// CHECK-NEXT:  [[val2:%\d+]] = OpVectorShuffle %v2int [[f2]] [[f2]] 0 1
// CHECK-NEXT:                  OpStore %a2 [[val2]]
  int2   a2 = t2.mips[2][pos2];

// CHECK-NEXT:[[pos3_0:%\d+]] = OpLoad %v3uint %pos3
// CHECK-NEXT:    [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:    [[f3:%\d+]] = OpImageFetch %v4uint [[t3]] [[pos3_0]] Lod %uint_3
// CHECK-NEXT:  [[val3:%\d+]] = OpVectorShuffle %v3uint [[f3]] [[f3]] 0 1 2
// CHECK-NEXT:                  OpStore %a3 [[val3]]
  uint3  a3 = t3.mips[3][pos3];

// CHECK-NEXT:    [[t4:%\d+]] = OpLoad %type_1d_image_array %t4
// CHECK-NEXT:    [[f4:%\d+]] = OpImageFetch %v4float [[t4]] [[cu12]] Lod %uint_4
// CHECK-NEXT:                  OpStore %a4 [[f4]]
  float4 a4 = t4.mips[4][uint2(1,2)];

// CHECK-NEXT:  [[pos0:%\d+]] = OpLoad %uint %pos
// CHECK-NEXT:[[pos3_1:%\d+]] = OpLoad %v3uint %pos3
// CHECK-NEXT:    [[t5:%\d+]] = OpLoad %type_2d_image_array %t5
// CHECK-NEXT:    [[f5:%\d+]] = OpImageFetch %v4int [[t5]] [[pos3_1]] Lod [[pos0]]
// CHECK-NEXT:  [[val5:%\d+]] = OpVectorShuffle %v3int [[f5]] [[f5]] 0 1 2
// CHECK-NEXT:                  OpStore %a5 [[val5]]
  int3   a5 = t5.mips[pos][pos3];
}
