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
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %int [[f1]] 0
  int int1 = intbuf.Load(address);

// CHECK:      [[f2:%\d+]] = OpImageFetch %v4uint %uintbuf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %uint [[f2]] 0
  uint uint1 = uintbuf.Load(address);

// CHECK:      [[f3:%\d+]] = OpImageFetch %v4float %floatbuf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpCompositeExtract %float [[f3]] 0
  float float1 = floatbuf.Load(address);

// CHECK:      [[f4:%\d+]] = OpImageFetch %v4int %int2buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2int [[f4]] [[f4]] 0 1
  int2 int2 = int2buf.Load(address);

// CHECK:      [[f5:%\d+]] = OpImageFetch %v4uint %uint2buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2uint [[f5]] [[f5]] 0 1
  uint2 uint2 = uint2buf.Load(address);

// CHECK:      [[f6:%\d+]] = OpImageFetch %v4float %float2buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v2float [[f6]] [[f6]] 0 1
  float2 float2 = float2buf.Load(address);

// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int %int3buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3int [[f7]] [[f7]] 0 1 2
  int3 int3 = int3buf.Load(address);

// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint %uint3buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3uint [[f8]] [[f8]] 0 1 2
  uint3 uint3 = uint3buf.Load(address);

// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float %float3buf {{%\d+}} None
// CHECK-NEXT: {{%\d+}} = OpVectorShuffle %v3float [[f9]] [[f9]] 0 1 2
  float3 float3 = float3buf.Load(address);

// CHECK:      {{%\d+}} = OpImageFetch %v4int %int4buf {{%\d+}} None
// CHECK-NEXT: OpStore %int4 {{%\d+}}
  int4 int4 = int4buf.Load(address);

// CHECK:      {{%\d+}} = OpImageFetch %v4uint %uint4buf {{%\d+}} None
// CHECK-NEXT: OpStore %uint4 {{%\d+}}
  uint4 uint4 = uint4buf.Load(address);

// CHECK:      {{%\d+}} = OpImageFetch %v4float %float4buf {{%\d+}} None
// CHECK-NEXT: OpStore %float4 {{%\d+}}
  float4 float4 = float4buf.Load(address);
}
