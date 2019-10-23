// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float t;                                      ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:     4
// CHECK: ;
// CHECK: ; }

// Make sure default val for cbuffer element is ignored.

// CHECK-NOT: float 5.000000e-01
// CHECK: @dx.op.cbufferLoad.f32
// CHECK: Sqrt

float t = 0.5;

float main() : SV_TARGET
{
  return sqrt(t);
}