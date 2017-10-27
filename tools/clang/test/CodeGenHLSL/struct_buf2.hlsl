// RUN: %dxc -E main -T ps_6_2

struct MyStruct {
  int   i1;
  int2  i2;
  int3  i3;
  int4  i4;
  uint  u1;
  uint2 u2;
  uint3 u3;
  uint4 u4;
  half  h1;
  half2 h2;
  half3 h3;
  half4 h4;
  float f1;
  float2 f2;
  float3 f3;
  float4 f4;
};

StructuredBuffer<MyStruct> buf1;
RWStructuredBuffer<MyStruct> buf2;

int4 main(float idx1 : IDX1, float idx2 : IDX2) : SV_Target {
  uint status;
  float4 r;
  r.x += buf1.LoadInt(idx1).i1;
  r.xy += buf1.LoadInt2(idx2).i2;
  r.xyz += buf1.LoadInt3(idx1).i3;
  r.xyzw += buf1.LoadInt4(idx2).i4;

  r.x += buf1.Load(idx1).u1;
  r.xy += buf1.Load2(idx2).u2;
  r.xyz += buf1.Load3(idx1).u3;
  r.xyzw += buf1.Load4(idx2).u4;

  r.x += buf1.LoadHalf(idx1).h1;
  r.xy += buf1.LoadHalf2(idx2).h2;
  r.xyz += buf1.LoadHalf3(idx1).h3;
  r.xyzw += buf1.LoadHalf4(idx2).h4;

  r.x += buf1.LoadFloat(idx1).f1;
  r.xy += buf1.LoadFloat2(idx2).f2;
  r.xyz += buf1.LoadFloat3(idx1).f3;
  r.xyzw += buf1.LoadFloat4(idx2).f4;

  r.x += buf2.LoadInt(idx1).i1;
  r.xy += buf2.LoadInt2(idx2).i2;
  r.xyz += buf2.LoadInt3(idx1).i3;
  r.xyzw += buf2.LoadInt4(idx2).i4;

  r.x += buf2.Load(idx1).u1;
  r.xy += buf2.Load2(idx2).u2;
  r.xyz += buf2.Load3(idx1).u3;
  r.xyzw += buf2.Load4(idx2).u4;

  r.x += buf2.LoadHalf(idx1).h1;
  r.xy += buf2.LoadHalf2(idx2).h2;
  r.xyz += buf2.LoadHalf3(idx1).h3;
  r.xyzw += buf2.LoadHalf4(idx2).h4;

  r.x += buf2.LoadFloat(idx1).f1;
  r.xy += buf2.LoadFloat2(idx2).f2;
  r.xyz += buf2.LoadFloat3(idx1).f3;
  r.xyzw += buf2.LoadFloat4(idx2).f4;

  return r;
}