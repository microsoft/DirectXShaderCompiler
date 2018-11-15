// RUN: %dxc -T ps_6_0 -E main -Od %s | FileCheck %s

// StructuredBuffer/RWStructuredBuffer should have the same layout

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 8
// CHECK: float2 c; ; Offset: 16
// CHECK: float2 d; ; Offset: 24
// CHECK: float e; ; Offset: 32
// CHECK: Size: 36

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 8
// CHECK: float2 c; ; Offset: 16
// CHECK: float2 d; ; Offset: 24
// CHECK: float e; ; Offset: 32
// CHECK: Size: 36

struct Struct
{
  float2 a;
  struct
  {
    float b[2];
    float2 c;
    float2 d;
  } s;
  float e;
};

StructuredBuffer<Struct> sb;
RWStructuredBuffer<Struct> rwsb;

float4 main() : SV_Target
{
    return sb[0].e + rwsb[0].e;
}