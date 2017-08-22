// Run: %dxc -T cs_6_0 -E main

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

[numthreads(1, 1, 1)]
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
// CHECK-NEXT: [[e1:%\d+]] = OpCompositeExtract %int [[f4]] 0
// CHECK-NEXT: [[e2:%\d+]] = OpCompositeExtract %int [[f4]] 1
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v2int [[e1]] [[e2]]

  int2 int2 = int2buf.Load(address);
// CHECK:      [[f5:%\d+]] = OpImageFetch %v4uint %uint2buf {{%\d+}} None
// CHECK-NEXT: [[e3:%\d+]] = OpCompositeExtract %uint [[f5]] 0
// CHECK-NEXT: [[e4:%\d+]] = OpCompositeExtract %uint [[f5]] 1
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v2uint [[e3]] [[e4]]
  uint2 uint2 = uint2buf.Load(address);

// CHECK:      [[f6:%\d+]] = OpImageFetch %v4float %float2buf {{%\d+}} None
// CHECK-NEXT: [[e5:%\d+]] = OpCompositeExtract %float [[f6]] 0
// CHECK-NEXT: [[e6:%\d+]] = OpCompositeExtract %float [[f6]] 1
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v2float [[e5]] [[e6]]
  float2 float2 = float2buf.Load(address);

// CHECK:      [[f7:%\d+]] = OpImageFetch %v4int %int3buf {{%\d+}} None
// CHECK-NEXT: [[e7:%\d+]] = OpCompositeExtract %int [[f7]] 0
// CHECK-NEXT: [[e8:%\d+]] = OpCompositeExtract %int [[f7]] 1
// CHECK-NEXT: [[e9:%\d+]] = OpCompositeExtract %int [[f7]] 2
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v3int [[e7]] [[e8]] [[e9]]
  int3 int3 = int3buf.Load(address);

// CHECK:      [[f8:%\d+]] = OpImageFetch %v4uint %uint3buf {{%\d+}} None
// CHECK-NEXT: [[e10:%\d+]] = OpCompositeExtract %uint [[f8]] 0
// CHECK-NEXT: [[e11:%\d+]] = OpCompositeExtract %uint [[f8]] 1
// CHECK-NEXT: [[e12:%\d+]] = OpCompositeExtract %uint [[f8]] 2
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v3uint [[e10]] [[e11]] [[e12]]
  uint3 uint3 = uint3buf.Load(address);

// CHECK:      [[f9:%\d+]] = OpImageFetch %v4float %float3buf {{%\d+}} None
// CHECK-NEXT: [[e13:%\d+]] = OpCompositeExtract %float [[f9]] 0
// CHECK-NEXT: [[e14:%\d+]] = OpCompositeExtract %float [[f9]] 1
// CHECK-NEXT: [[e15:%\d+]] = OpCompositeExtract %float [[f9]] 2
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v3float [[e13]] [[e14]] [[e15]]
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
