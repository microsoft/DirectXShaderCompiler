// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float A[6];                                   ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:    24
// CHECK: ;
// CHECK: ; }

// Make sure no lshr created for cbuffer array.
// CHECK-NOT: lshr
// CHECK: shl
// CHECK: call float @dx.op.cbufferLoad.f32
// CHECK: call float @dx.op.cbufferLoad.f32
// CHECK: call float @dx.op.cbufferLoad.f32

float A[6] : register(b0);
float main(int i : A) : SV_TARGET
{
  return A[i] + A[i+1] + A[i+2] ;
}
