// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float4 g1;                                    ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:    16
// CHECK: ;
// CHECK: ; }

// CHECK: @main

// CHECK: @dx.op.cbufferLoad.f32
// CHECK: i32 4
// CHECK: @dx.op.cbufferLoad.f32
// CHECK: i32 12

float4 g1;

float4 main() : SV_TARGET
{
  return g1.wyyy;
}
