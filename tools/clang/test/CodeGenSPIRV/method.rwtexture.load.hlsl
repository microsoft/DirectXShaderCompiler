// Run: %dxc -T ps_6_0 -E main

RWTexture1D<int> intbuf;
RWTexture2D<uint2> uint2buf;
RWTexture3D<float3> float3buf;
RWTexture1DArray<float4> float4buf;
RWTexture2DArray<int3> int3buf;

void main() {

// CHECK:      [[img1:%\d+]] = OpLoad %type_1d_image %intbuf
// CHECK-NEXT: [[ret1:%\d+]] = OpImageRead %v4int [[img1]] %int_0 None
// CHECK-NEXT: [[r1:%\d+]] = OpCompositeExtract %int [[ret1]] 0
// CHECK-NEXT: OpStore %a [[r1]]
  int a    = intbuf.Load(0);

// CHECK-NEXT: [[img2:%\d+]] = OpLoad %type_2d_image %uint2buf
// CHECK-NEXT: [[ret2:%\d+]] = OpImageRead %v4uint [[img2]] {{%\d+}} None
// CHECK-NEXT: [[r2:%\d+]] = OpVectorShuffle %v2uint [[ret2]] [[ret2]] 0 1
// CHECK-NEXT: OpStore %b [[r2]]
  uint2 b  = uint2buf.Load(0);

// CHECK-NEXT: [[img3:%\d+]] = OpLoad %type_3d_image %float3buf
// CHECK-NEXT: [[ret3:%\d+]] = OpImageRead %v4float [[img3]] {{%\d+}} None
// CHECK-NEXT: [[r3:%\d+]] = OpVectorShuffle %v3float [[ret3]] [[ret3]] 0 1 2
// CHECK-NEXT: OpStore %c [[r3]]
  float3 c = float3buf.Load(0);

// CHECK-NEXT: [[img4:%\d+]] = OpLoad %type_1d_image_array %float4buf
// CHECK-NEXT: [[r4:%\d+]] = OpImageRead %v4float [[img4]] {{%\d+}} None
// CHECK-NEXT: OpStore %d [[r4]]
  float4 d = float4buf.Load(0);

// CHECK-NEXT: [[img5:%\d+]] = OpLoad %type_2d_image_array %int3buf
// CHECK-NEXT: [[ret5:%\d+]] = OpImageRead %v4int [[img5]] {{%\d+}} None
// CHECK-NEXT: [[r5:%\d+]] = OpVectorShuffle %v3int [[ret5]] [[ret5]] 0 1 2
// CHECK-NEXT: OpStore %e [[r5]]
  int3 e   = int3buf.Load(0);
}
