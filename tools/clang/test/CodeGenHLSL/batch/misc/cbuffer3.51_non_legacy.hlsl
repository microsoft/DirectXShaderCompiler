// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer buf1
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct buf1
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Foo
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 g1[16];                            ; Offset:    0
// CHECK: ;
// CHECK: ;       } buf1;                                       ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } buf1;                                           ; Offset:    0 Size:   256
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; cbuffer buf2
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct buf2
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Bar
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           uint3 idx[16];                            ; Offset:    0
// CHECK: ;
// CHECK: ;       } buf2;                                       ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } buf2;                                           ; Offset:    0 Size:   192
// CHECK: ;
// CHECK: ; }

// CHECK: @main

// CHECK: call i32 @dx.op.cbufferLoad.i32
// CHECK: call float @dx.op.cbufferLoad.f32
// CHECK: call float @dx.op.cbufferLoad.f32

struct Foo
{
  float4 g1[16];
};

struct Bar
{
  uint3 idx[16];
};

ConstantBuffer<Foo> buf1[32] : register(b77, space3);
ConstantBuffer<Bar> buf2[64] : register(b17);

float4 main(int3 a : A) : SV_TARGET
{
  return buf1[ buf2[a.x].idx[a.y].z ].g1[a.z + 12].wyyy;
}
