// RUN: %dxc -T ps_6_0 -E main -Od %s | FileCheck %s

// All cbuffer and tbuffer declarations should have the same layout
// We don't care in what order they get printed

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 16
// CHECK: float2 c; ; Offset: 36
// CHECK: float2 d; ; Offset: 48
// CHECK: float e; ; Offset: 56
// CHECK: Size: 60

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 16
// CHECK: float2 c; ; Offset: 36
// CHECK: float2 d; ; Offset: 48
// CHECK: float e; ; Offset: 56
// CHECK: Size: 60

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 16
// CHECK: float2 c; ; Offset: 36
// CHECK: float2 d; ; Offset: 48
// CHECK: float e; ; Offset: 56
// CHECK: Size: 60

// CHECK: float2 a; ; Offset: 0
// CHECK: float b[2]; ; Offset: 16
// CHECK: float2 c; ; Offset: 36
// CHECK: float2 d; ; Offset: 48
// CHECK: float e; ; Offset: 56
// CHECK: Size: 60

struct Struct
{
  float2 a;
  struct
  {
    float b[2]; // Each element is float4-aligned
    float2 c; // Fits in b[1].yz
    float2 d; // Doesn't fit in b[1].w-, so gets its own float4
  } s;
  float e; // Fits in d.z
};

cbuffer _cbl
{
  Struct cbl;
};
ConstantBuffer<Struct> cb;

tbuffer _tbl
{
  Struct tbl;
};
TextureBuffer<Struct> tb;

float4 main() : SV_Target
{
    return cbl.e + cb.e + tbl.e + tb.e;
}