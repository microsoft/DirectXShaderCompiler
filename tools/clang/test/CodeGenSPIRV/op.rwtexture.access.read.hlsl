// Run: %dxc -T ps_6_0 -E main

RWTexture1D      <float>  t1;
RWTexture2D      <int2>   t2;
RWTexture3D      <uint3>  t3;
RWTexture1DArray <float4> t4;
RWTexture2DArray <int3>   t5;

// CHECK:  [[c01:%\d+]] = OpConstantComposite %v2uint %uint_0 %uint_1
// CHECK: [[c012:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_1 %uint_2

void main() {

// CHECK:      [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT: [[img1:%\d+]] = OpImageRead %float [[t1]] %uint_0 None
// CHECK-NEXT: OpStore %a [[img1]]
  float  a = t1[0];

// CHECK-NEXT: [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT: [[img2:%\d+]] = OpImageRead %v2int [[t2]] [[c01]] None
// CHECK-NEXT: OpStore %b [[img2]]
  int2   b = t2[uint2(0,1)];

// CHECK-NEXT: [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT: [[img3:%\d+]] = OpImageRead %v3uint [[t3]] [[c012]] None
// CHECK-NEXT: OpStore %c [[img3]]
  uint3  c = t3[uint3(0,1,2)];

// CHECK-NEXT: [[t4:%\d+]] = OpLoad %type_1d_image_array %t4
// CHECK-NEXT: [[img4:%\d+]] = OpImageRead %v4float [[t4]] [[c01]] None
// CHECK-NEXT: OpStore %d [[img4]]
  float4 d = t4[uint2(0,1)];

// CHECK-NEXT: [[t5:%\d+]] = OpLoad %type_2d_image_array %t5
// CHECK-NEXT: [[img5:%\d+]] = OpImageRead %v3int [[t5]] [[c012]] None
// CHECK-NEXT: OpStore %e [[img5]]
  int3   e = t5[uint3(0,1,2)];
}
