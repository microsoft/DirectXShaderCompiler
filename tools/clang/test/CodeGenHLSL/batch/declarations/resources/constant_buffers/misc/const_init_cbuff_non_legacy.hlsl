// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float c;                                      ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:     4
// CHECK: ;
// CHECK: ; }

// Make sure default val for cbuffer element is ignored.
// CHECK: call float @dx.op.cbufferLoad.f32
// CHECK: fadd

float c = 0.91;

float main() : SV_Target {
  const float x = 1+c;
  return x;
}