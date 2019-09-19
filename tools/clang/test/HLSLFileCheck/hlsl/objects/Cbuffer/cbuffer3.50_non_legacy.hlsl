// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer Foo
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct Foo
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float4 g1[16];                                ; Offset:    0
// CHECK: ;
// CHECK: ;   } Foo;                                            ; Offset:    0 Size:   256
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; cbuffer Bar
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct Bar
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       uint3 idx[8];                                 ; Offset:    0
// CHECK: ;
// CHECK: ;   } Bar;                                            ; Offset:    0 Size:    96
// CHECK: ;
// CHECK: ; }

// CHECK: @main

// CHECK: call i32 @dx.op.cbufferLoad.i32
// CHECK: call float @dx.op.cbufferLoad.f32
// CHECK: call float @dx.op.cbufferLoad.f32

cbuffer Foo
{
  float4 g1[16];
};

cbuffer Bar
{
  uint3 idx[8];
};

float4 main(int2 a : A) : SV_TARGET
{
  return g1[idx[a.x].z].wyyy;
}
