// Run: %dxc -T ps_6_0 -E main

Buffer<int> intbuf;
Buffer<uint> uintbuf;
Buffer<float> floatbuf;
RWBuffer<int2> int2buf;
RWBuffer<uint2> uint2buf;
RWBuffer<float2> float2buf;
Buffer<int3> int3buf;
Buffer<uint3> uint3buf;
Buffer<float3> float3buf;
RWBuffer<int4> int4buf;
RWBuffer<uint4> uint4buf;
RWBuffer<float4> float4buf;

void main() {
  int address;

// CHECK:      [[img1:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK:      [[f1:%\d+]] = OpImageFetch %v4int [[img1]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %int [[f1]] 0
  int int1 = intbuf.Load(address);

// CHECK:      [[img2:%\d+]] = OpLoad %type_buffer_image_0 %uintbuf
// CHECK:      [[f2:%\d+]] = OpImageFetch %v4uint [[img2]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %uint [[f2]] 0
  uint uint1 = uintbuf.Load(address);

// CHECK:      [[img3:%\d+]] = OpLoad %type_buffer_image_1 %floatbuf
// CHECK:      [[f3:%\d+]] = OpImageFetch %v4float [[img3]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %float [[f3]] 0
  float float1 = floatbuf.Load(address);

// CHECK:      [[img4:%\d+]] = OpLoad %type_buffer_image_2 %int2buf
// CHECK:      {{%\d+}} = OpImageRead %v2int [[img4]] {{%\d+}} None
  int2 int2 = int2buf.Load(address);

// CHECK:      [[img5:%\d+]] = OpLoad %type_buffer_image_3 %uint2buf
// CHECK:      {{%\d+}} = OpImageRead %v2uint [[img5]] {{%\d+}} None
  uint2 uint2 = uint2buf.Load(address);

// CHECK:      [[img6:%\d+]] = OpLoad %type_buffer_image_4 %float2buf
// CHECK:      {{%\d+}} = OpImageRead %v2float [[img6]] {{%\d+}} None
  float2 float2 = float2buf.Load(address);

// CHECK:      [[img7:%\d+]] = OpLoad %type_buffer_image_5 %int3buf
// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int [[img7]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3int [[f7]] [[f7]] 0 1 2
  int3 int3 = int3buf.Load(address);

// CHECK:      [[img8:%\d+]] = OpLoad %type_buffer_image_6 %uint3buf
// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint [[img8]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3uint [[f8]] [[f8]] 0 1 2
  uint3 uint3 = uint3buf.Load(address);

// CHECK:      [[img9:%\d+]] = OpLoad %type_buffer_image_7 %float3buf
// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float [[img9]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3float [[f9]] [[f9]] 0 1 2
  float3 float3 = float3buf.Load(address);

// CHECK:      [[img10:%\d+]] = OpLoad %type_buffer_image_8 %int4buf
// CHECK:      {{%\d+}} = OpImageRead %v4int [[img10]] {{%\d+}} None
// CHECK-NEXT: OpStore %int4 {{%\d+}}
  int4 int4 = int4buf.Load(address);

// CHECK:      [[img11:%\d+]] = OpLoad %type_buffer_image_9 %uint4buf
// CHECK:      {{%\d+}} = OpImageRead %v4uint [[img11]] {{%\d+}} None
// CHECK-NEXT: OpStore %uint4 {{%\d+}}
  uint4 uint4 = uint4buf.Load(address);

// CHECK:      [[img12:%\d+]] = OpLoad %type_buffer_image_10 %float4buf
// CHECK:      {{%\d+}} = OpImageRead %v4float [[img12]] {{%\d+}} None
// CHECK-NEXT: OpStore %float4 {{%\d+}}
  float4 float4 = float4buf.Load(address);
}
