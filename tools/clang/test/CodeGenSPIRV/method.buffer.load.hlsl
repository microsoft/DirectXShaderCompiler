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

// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

void main() {
  int address;

// CHECK:      [[img1:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK:      [[f1:%\d+]] = OpImageFetch %v4int [[img1]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %int [[f1]] 0
  int i1 = intbuf.Load(address);

// CHECK:      [[img2:%\d+]] = OpLoad %type_buffer_image_0 %uintbuf
// CHECK:      [[f2:%\d+]] = OpImageFetch %v4uint [[img2]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %uint [[f2]] 0
  uint u1 = uintbuf.Load(address);

// CHECK:      [[img3:%\d+]] = OpLoad %type_buffer_image_1 %floatbuf
// CHECK:      [[f3:%\d+]] = OpImageFetch %v4float [[img3]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %float [[f3]] 0
  float f1 = floatbuf.Load(address);

// CHECK:      [[img4:%\d+]] = OpLoad %type_buffer_image_2 %int2buf
// CHECK:      [[f4:%\d+]] = OpImageRead %v4int [[img4]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2int [[f4]] [[f4]] 0 1
  int2 i2 = int2buf.Load(address);

// CHECK:      [[img5:%\d+]] = OpLoad %type_buffer_image_3 %uint2buf
// CHECK:      [[f5:%\d+]] = OpImageRead %v4uint [[img5]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2uint [[f5]] [[f5]] 0 1
  uint2 u2 = uint2buf.Load(address);

// CHECK:      [[img6:%\d+]] = OpLoad %type_buffer_image_4 %float2buf
// CHECK:      [[f6:%\d+]] = OpImageRead %v4float [[img6]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2float [[f6]] [[f6]] 0 1
  float2 f2 = float2buf.Load(address);

// CHECK:      [[img7:%\d+]] = OpLoad %type_buffer_image_5 %int3buf
// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int [[img7]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3int [[f7]] [[f7]] 0 1 2
  int3 i3 = int3buf.Load(address);

// CHECK:      [[img8:%\d+]] = OpLoad %type_buffer_image_6 %uint3buf
// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint [[img8]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3uint [[f8]] [[f8]] 0 1 2
  uint3 u3 = uint3buf.Load(address);

// CHECK:      [[img9:%\d+]] = OpLoad %type_buffer_image_7 %float3buf
// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float [[img9]] {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3float [[f9]] [[f9]] 0 1 2
  float3 f3 = float3buf.Load(address);

// CHECK:      [[img10:%\d+]] = OpLoad %type_buffer_image_8 %int4buf
// CHECK:      {{%\d+}} = OpImageRead %v4int [[img10]] {{%\d+}} None
// CHECK-NEXT: OpStore %i4 {{%\d+}}
  int4 i4 = int4buf.Load(address);

// CHECK:      [[img11:%\d+]] = OpLoad %type_buffer_image_9 %uint4buf
// CHECK:      {{%\d+}} = OpImageRead %v4uint [[img11]] {{%\d+}} None
// CHECK-NEXT: OpStore %u4 {{%\d+}}
  uint4 u4 = uint4buf.Load(address);

// CHECK:      [[img12:%\d+]] = OpLoad %type_buffer_image_10 %float4buf
// CHECK:      {{%\d+}} = OpImageRead %v4float [[img12]] {{%\d+}} None
// CHECK-NEXT: OpStore %f4 {{%\d+}}
  float4 f4 = float4buf.Load(address);

///////////////////////////////
// Using the Status argument //  
///////////////////////////////
  uint status;

// CHECK:              [[img3:%\d+]] = OpLoad %type_buffer_image_1 %floatbuf
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct [[img3]] {{%\d+}} None
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[v4result]] 0
// CHECK-NEXT:                         OpStore %r1 [[result]]
  float  r1 = floatbuf.Load(address, status);  // Test for Buffer

// CHECK:              [[img6:%\d+]] = OpLoad %type_buffer_image_4 %float2buf
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseRead %SparseResidencyStruct [[img6]] {{%\d+}} None
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:       [[result:%\d+]] = OpVectorShuffle %v2float [[v4result]] [[v4result]] 0 1
// CHECK-NEXT:                         OpStore %r2 [[result]]
  float2 r2 = float2buf.Load(address, status);  // Test for RWBuffer
}
