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

// CHECK:      [[f1:%\d+]] = OpImageFetch %v4int %intbuf {{%\d+}} None
// CHECK-NEXT: [[r1:%\d+]] = OpCompositeExtract %int [[f1]] 0
// CHECK-NEXT: OpStore %int1 [[r1]]
  int int1 = intbuf[address];

// CHECK:      [[f2:%\d+]] = OpImageFetch %v4uint %uintbuf {{%\d+}} None
// CHECK-NEXT: [[r2:%\d+]] = OpCompositeExtract %uint [[f2]] 0
// CHECK-NEXT: OpStore %uint1 [[r2]]
  uint uint1 = uintbuf[address];

// CHECK:      [[f3:%\d+]] = OpImageFetch %v4float %floatbuf {{%\d+}} None
// CHECK-NEXT: [[r3:%\d+]] = OpCompositeExtract %float [[f3]] 0
// CHECK-NEXT: OpStore %float1 [[r3]]
  float float1 = floatbuf[address];

// CHECK:      [[f4:%\d+]] = OpImageFetch %v4int %int2buf {{%\d+}} None
// CHECK-NEXT: [[r4:%\d+]] = OpVectorShuffle %v2int [[f4]] [[f4]] 0 1
// CHECK-NEXT: OpStore %int2 [[r4]]
  int2 int2 = int2buf[address];

// CHECK:      [[f5:%\d+]] = OpImageFetch %v4uint %uint2buf {{%\d+}} None
// CHECK-NEXT: [[r5:%\d+]] = OpVectorShuffle %v2uint [[f5]] [[f5]] 0 1
// CHECK-NEXT: OpStore %uint2 [[r5]]
  uint2 uint2 = uint2buf[address];

// CHECK:      [[f6:%\d+]] = OpImageFetch %v4float %float2buf {{%\d+}} None
// CHECK-NEXT: [[r6:%\d+]] = OpVectorShuffle %v2float [[f6]] [[f6]] 0 1
// CHECK-NEXT: OpStore %float2 [[r6]]
  float2 float2 = float2buf[address];

// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int %int3buf {{%\d+}} None
// CHECK-NEXT: [[r7:%\d+]] = OpVectorShuffle %v3int [[f7]] [[f7]] 0 1 2
// CHECK-NEXT: OpStore %int3 [[r7]]
  int3 int3 = int3buf[address];

// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint %uint3buf {{%\d+}} None
// CHECK-NEXT: [[r8:%\d+]] = OpVectorShuffle %v3uint [[f8]] [[f8]] 0 1 2
// CHECK-NEXT: OpStore %uint3 [[r8]]
  uint3 uint3 = uint3buf[address];

// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float %float3buf {{%\d+}} None
// CHECK-NEXT: [[r9:%\d+]] = OpVectorShuffle %v3float [[f9]] [[f9]] 0 1 2
// CHECK-NEXT: OpStore %float3 [[r9]]
  float3 float3 = float3buf[address];

// CHECK:      [[r10:%\d+]] = OpImageFetch %v4int %int4buf {{%\d+}} None
// CHECK-NEXT: OpStore %int4 [[r10]]
  int4 int4 = int4buf[address];

// CHECK:      [[r11:%\d+]] = OpImageFetch %v4uint %uint4buf {{%\d+}} None
// CHECK-NEXT: OpStore %uint4 [[r11]]
  uint4 uint4 = uint4buf[address];

// CHECK:      [[r12:%\d+]] = OpImageFetch %v4float %float4buf {{%\d+}} None
// CHECK-NEXT: OpStore %float4 [[r12]]
  float4 float4 = float4buf[address];
}
