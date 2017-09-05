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

// CHECK: [[int_1_2:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[uint_3_4:%\d+]] = OpConstantComposite %v2uint %uint_3 %uint_4
// CHECK: [[float_5_6:%\d+]] = OpConstantComposite %v2float %float_5 %float_6
// CHECK: [[int_1_2_3:%\d+]] = OpConstantComposite %v3int %int_1 %int_2 %int_3
// CHECK: [[uint_4_5_6:%\d+]] = OpConstantComposite %v3uint %uint_4 %uint_5 %uint_6
// CHECK: [[float_7_8_9:%\d+]] = OpConstantComposite %v3float %float_7 %float_8 %float_9
// CHECK: [[int_1_2_3_4:%\d+]] = OpConstantComposite %v4int %int_1 %int_2 %int_3 %int_4
// CHECK: [[uint_5_6_7_8:%\d+]] = OpConstantComposite %v4uint %uint_5 %uint_6 %uint_7 %uint_8
// CHECK: [[float_9_10_11_12:%\d+]] = OpConstantComposite %v4float %float_9 %float_10 %float_11 %float_12

void main() {

// CHECK:      [[img1:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK-NEXT: OpImageWrite [[img1]] %uint_1 %int_1
  intbuf[1] = int(1);

// CHECK:      [[img2:%\d+]] = OpLoad %type_buffer_image_0 %uintbuf
// CHECK-NEXT: OpImageWrite [[img2]] %uint_2 %uint_2
  uintbuf[2] = uint(2);

// CHECK:      [[img3:%\d+]] = OpLoad %type_buffer_image_1 %floatbuf
// CHECK-NEXT: OpImageWrite [[img3]] %uint_3 %float_3
  floatbuf[3] = float(3);

// CHECK:      [[img4:%\d+]] = OpLoad %type_buffer_image_2 %int2buf
// CHECK-NEXT: OpImageWrite [[img4]] %uint_4 [[int_1_2]]
  int2buf[4] = int2(1,2);

// CHECK:      [[img5:%\d+]] = OpLoad %type_buffer_image_3 %uint2buf
// CHECK-NEXT: OpImageWrite [[img5]] %uint_5 [[uint_3_4]]
  uint2buf[5] = uint2(3,4);

// CHECK:      [[img6:%\d+]] = OpLoad %type_buffer_image_4 %float2buf
// CHECK-NEXT: OpImageWrite [[img6]] %uint_6 [[float_5_6]]
  float2buf[6] = float2(5,6);

// CHECK:      [[img7:%\d+]] = OpLoad %type_buffer_image_5 %int3buf
// CHECK-NEXT: OpImageWrite [[img7]] %uint_7 [[int_1_2_3]]
  int3buf[7] = int3(1,2,3);

// CHECK:      [[img8:%\d+]] = OpLoad %type_buffer_image_6 %uint3buf
// CHECK-NEXT: OpImageWrite [[img8]] %uint_8 [[uint_4_5_6]]
  uint3buf[8] = uint3(4,5,6);

// CHECK:      [[img9:%\d+]] = OpLoad %type_buffer_image_7 %float3buf
// CHECK-NEXT: OpImageWrite [[img9]] %uint_9 [[float_7_8_9]]
  float3buf[9] = float3(7,8,9);

// CHECK:      [[img10:%\d+]] = OpLoad %type_buffer_image_8 %int4buf
// CHECK-NEXT: OpImageWrite [[img10]] %uint_10 [[int_1_2_3_4]]
  int4buf[10] = int4(1,2,3,4);

// CHECK:      [[img11:%\d+]] = OpLoad %type_buffer_image_9 %uint4buf
// CHECK-NEXT: OpImageWrite [[img11]] %uint_11 [[uint_5_6_7_8]]
  uint4buf[11] = uint4(5,6,7,8);

// CHECK:      [[img12:%\d+]] = OpLoad %type_buffer_image_10 %float4buf
// CHECK-NEXT: OpImageWrite [[img12]] %uint_12 [[float_9_10_11_12]]
  float4buf[12] = float4(9,10,11,12);
}
