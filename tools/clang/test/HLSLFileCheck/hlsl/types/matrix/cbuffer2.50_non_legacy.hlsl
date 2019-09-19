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

// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %"$Globals_cbuffer", i32 4, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %"$Globals_cbuffer", i32 12, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)

float4 g1;

float4 main() : SV_TARGET
{
  return g1.wyyy;
}
