// RUN: %dxc -T vs_6_0 -E main -not_use_legacy_cbuf_load %s | FileCheck %s

// Test that register annotations on globals constants affect the offset.

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       int x;                                        ; Offset:   16
// CHECK: ;       int y;                                        ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:    20
// CHECK: ;
// CHECK: ; }

// CHECK: @dx.op.cbufferLoad.i32
// CHECK: i32 16
// CHECK: @dx.op.cbufferLoad.i32
// CHECK: i32 0

int x : register(c1);
int y : register(c0);

int2 main() : OUT { return int2(x, y); }