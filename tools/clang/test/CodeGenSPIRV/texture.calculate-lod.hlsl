// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ImageQuery

SamplerState ss : register(s2);

Texture1D        <float>  t1;
Texture1DArray   <float4> t2;
Texture2D        <int2>   t3;
Texture2DArray   <int3>   t4;
Texture3D        <uint3>  t5;
TextureCube      <int>    t6;
TextureCubeArray <int>    t7;

// CHECK:   %type_sampled_image = OpTypeSampledImage %type_1d_image
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_1d_image_array
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_2d_image
// CHECK: %type_sampled_image_2 = OpTypeSampledImage %type_2d_image_array
// CHECK: %type_sampled_image_3 = OpTypeSampledImage %type_3d_image
// CHECK: %type_sampled_image_4 = OpTypeSampledImage %type_cube_image
// CHECK: %type_sampled_image_5 = OpTypeSampledImage %type_cube_image_array

void main() {
  float x = 0.5;
  float2 xy = float2(0.5, 0.5);
  float3 xyz = float3(0.5, 0.5, 0.5);

//CHECK:          [[t1:%\d+]] = OpLoad %type_1d_image %t1
//CHECK-NEXT:    [[ss1:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:     [[x1:%\d+]] = OpLoad %float %x
//CHECK-NEXT:    [[si1:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[ss1]]
//CHECK-NEXT: [[query1:%\d+]] = OpImageQueryLod %v2float [[si1]] [[x1]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query1]] 0
  float lod1 = t1.CalculateLevelOfDetail(ss, x);

//CHECK:          [[t2:%\d+]] = OpLoad %type_1d_image_array %t2
//CHECK-NEXT:    [[ss2:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:     [[x2:%\d+]] = OpLoad %float %x
//CHECK-NEXT:    [[si2:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[ss2]]
//CHECK-NEXT: [[query2:%\d+]] = OpImageQueryLod %v2float [[si2]] [[x2]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query2]] 0
  float lod2 = t2.CalculateLevelOfDetail(ss, x);

//CHECK:          [[t3:%\d+]] = OpLoad %type_2d_image %t3
//CHECK-NEXT:    [[ss3:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:    [[xy1:%\d+]] = OpLoad %v2float %xy
//CHECK-NEXT:    [[si3:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[ss3]]
//CHECK-NEXT: [[query3:%\d+]] = OpImageQueryLod %v2float [[si3]] [[xy1]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query3]] 0
  float lod3 = t3.CalculateLevelOfDetail(ss, xy);

//CHECK:          [[t4:%\d+]] = OpLoad %type_2d_image_array %t4
//CHECK-NEXT:    [[ss4:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:    [[xy2:%\d+]] = OpLoad %v2float %xy
//CHECK-NEXT:    [[si4:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[ss4]]
//CHECK-NEXT: [[query4:%\d+]] = OpImageQueryLod %v2float [[si4]] [[xy2]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query4]] 0
  float lod4 = t4.CalculateLevelOfDetail(ss, xy);

//CHECK:          [[t5:%\d+]] = OpLoad %type_3d_image %t5
//CHECK-NEXT:    [[ss5:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:   [[xyz1:%\d+]] = OpLoad %v3float %xyz
//CHECK-NEXT:    [[si5:%\d+]] = OpSampledImage %type_sampled_image_3 [[t5]] [[ss5]]
//CHECK-NEXT: [[query5:%\d+]] = OpImageQueryLod %v2float [[si5]] [[xyz1]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query5]] 0
  float lod5 = t5.CalculateLevelOfDetail(ss, xyz);

//CHECK:          [[t6:%\d+]] = OpLoad %type_cube_image %t6
//CHECK-NEXT:    [[ss6:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:   [[xyz2:%\d+]] = OpLoad %v3float %xyz
//CHECK-NEXT:    [[si6:%\d+]] = OpSampledImage %type_sampled_image_4 [[t6]] [[ss6]]
//CHECK-NEXT: [[query6:%\d+]] = OpImageQueryLod %v2float [[si6]] [[xyz2]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query6]] 0
  float lod6 = t6.CalculateLevelOfDetail(ss, xyz);

//CHECK:          [[t7:%\d+]] = OpLoad %type_cube_image_array %t7
//CHECK-NEXT:    [[ss7:%\d+]] = OpLoad %type_sampler %ss
//CHECK-NEXT:   [[xyz3:%\d+]] = OpLoad %v3float %xyz
//CHECK-NEXT:    [[si7:%\d+]] = OpSampledImage %type_sampled_image_5 [[t7]] [[ss7]]
//CHECK-NEXT: [[query7:%\d+]] = OpImageQueryLod %v2float [[si7]] [[xyz3]]
//CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[query7]] 0
  float lod7 = t7.CalculateLevelOfDetail(ss, xyz);
}
