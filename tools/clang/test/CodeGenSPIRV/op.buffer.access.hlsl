// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ImageBuffer

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
// CHECK-NEXT: [[r1:%\d+]] = OpCompositeExtract %int [[f1]] 0
// CHECK-NEXT: OpStore %int1 [[r1]]
  int int1 = intbuf[address];

// CHECK:      [[img2:%\d+]] = OpLoad %type_buffer_image_0 %uintbuf
// CHECK:      [[f2:%\d+]] = OpImageFetch %v4uint [[img2]] {{%\d+}} None
// CHECK-NEXT: [[r2:%\d+]] = OpCompositeExtract %uint [[f2]] 0
// CHECK-NEXT: OpStore %uint1 [[r2]]
  uint uint1 = uintbuf[address];

// CHECK:      [[img3:%\d+]] = OpLoad %type_buffer_image_1 %floatbuf
// CHECK:      [[f3:%\d+]] = OpImageFetch %v4float [[img3]] {{%\d+}} None
// CHECK-NEXT: [[r3:%\d+]] = OpCompositeExtract %float [[f3]] 0
// CHECK-NEXT: OpStore %float1 [[r3]]
  float float1 = floatbuf[address];

// CHECK:      [[img4:%\d+]] = OpLoad %type_buffer_image_2 %int2buf
// CHECK:      [[ret4:%\d+]] = OpImageRead %v4int [[img4]] {{%\d+}} None
// CHECK-NEXT: [[r4:%\d+]] = OpVectorShuffle %v2int [[ret4]] [[ret4]] 0 1
// CHECK-NEXT: OpStore %int2 [[r4]]
  int2 int2 = int2buf[address];

// CHECK:      [[img5:%\d+]] = OpLoad %type_buffer_image_3 %uint2buf
// CHECK:      [[ret5:%\d+]] = OpImageRead %v4uint [[img5]] {{%\d+}} None
// CHECK-NEXT: [[r5:%\d+]] = OpVectorShuffle %v2uint [[ret5]] [[ret5]] 0 1
// CHECK-NEXT: OpStore %uint2 [[r5]]
  uint2 uint2 = uint2buf[address];

// CHECK:      [[img6:%\d+]] = OpLoad %type_buffer_image_4 %float2buf
// CHECK:      [[ret6:%\d+]] = OpImageRead %v4float [[img6]] {{%\d+}} None
// CHECK-NEXT: [[r6:%\d+]] = OpVectorShuffle %v2float [[ret6]] [[ret6]] 0 1
// CHECK-NEXT: OpStore %float2 [[r6]]
  float2 float2 = float2buf[address];

// CHECK:      [[img7:%\d+]] = OpLoad %type_buffer_image_5 %int3buf
// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int [[img7]] {{%\d+}} None
// CHECK-NEXT: [[r7:%\d+]] = OpVectorShuffle %v3int [[f7]] [[f7]] 0 1 2
// CHECK-NEXT: OpStore %int3 [[r7]]
  int3 int3 = int3buf[address];

// CHECK:      [[img8:%\d+]] = OpLoad %type_buffer_image_6 %uint3buf
// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint [[img8]] {{%\d+}} None
// CHECK-NEXT: [[r8:%\d+]] = OpVectorShuffle %v3uint [[f8]] [[f8]] 0 1 2
// CHECK-NEXT: OpStore %uint3 [[r8]]
  uint3 uint3 = uint3buf[address];

// CHECK:      [[img9:%\d+]] = OpLoad %type_buffer_image_7 %float3buf
// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float [[img9]] {{%\d+}} None
// CHECK-NEXT: [[r9:%\d+]] = OpVectorShuffle %v3float [[f9]] [[f9]] 0 1 2
// CHECK-NEXT: OpStore %float3 [[r9]]
  float3 float3 = float3buf[address];

// CHECK:      [[img10:%\d+]] = OpLoad %type_buffer_image_8 %int4buf
// CHECK:      [[r10:%\d+]] = OpImageRead %v4int [[img10]] {{%\d+}} None
// CHECK-NEXT: OpStore %int4 [[r10]]
  int4 int4 = int4buf[address];

// CHECK:      [[img11:%\d+]] = OpLoad %type_buffer_image_9 %uint4buf
// CHECK:      [[r11:%\d+]] = OpImageRead %v4uint [[img11]] {{%\d+}} None
// CHECK-NEXT: OpStore %uint4 [[r11]]
  uint4 uint4 = uint4buf[address];

// CHECK:      [[img12:%\d+]] = OpLoad %type_buffer_image_10 %float4buf
// CHECK:      [[r12:%\d+]] = OpImageRead %v4float [[img12]] {{%\d+}} None
// CHECK-NEXT: OpStore %float4 [[r12]]
  float4 float4 = float4buf[address];

// CHECK:      [[img13:%\d+]] = OpLoad %type_buffer_image_5 %int3buf
// CHECK-NEXT:   [[f13:%\d+]] = OpImageFetch %v4int [[img13]] %uint_0 None
// CHECK-NEXT:   [[r13:%\d+]] = OpVectorShuffle %v3int [[f13]] [[f13]] 0 1 2
// CHECK-NEXT:                  OpStore %temp_var_vector [[r13]]
// CHECK-NEXT:  [[ac13:%\d+]] = OpAccessChain %_ptr_Function_int %temp_var_vector %uint_1
// CHECK-NEXT:     [[a:%\d+]] = OpLoad %int [[ac13]]
// CHECK-NEXT:                  OpStore %a [[a]]
  int   a = int3buf[0][1];
// CHECK:      [[img14:%\d+]] = OpLoad %type_buffer_image_10 %float4buf
// CHECK-NEXT:   [[f14:%\d+]] = OpImageRead %v4float [[img14]] {{%\d+}} None
// CHECK-NEXT:                  OpStore %temp_var_vector_0 [[f14]]
// CHECK-NEXT:  [[ac14:%\d+]] = OpAccessChain %_ptr_Function_float %temp_var_vector_0 %uint_2
// CHECK-NEXT:     [[b:%\d+]] = OpLoad %float [[ac14]]
// CHECK-NEXT:                  OpStore %b [[b]]
  float b = float4buf[address][2];
}
