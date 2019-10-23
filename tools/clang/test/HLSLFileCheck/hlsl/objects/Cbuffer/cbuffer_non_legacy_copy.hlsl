// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// Make sure no alloca to copy.
// CHECK-NOT: alloca

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       uint i;                                       ; Offset:    0
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:     4
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; cbuffer T
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct T
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float4 a[2];                                  ; Offset:    0
// CHECK: ;       float4 b[2];                                  ; Offset:   32
// CHECK: ;
// CHECK: ;   } T;                                              ; Offset:    0 Size:    64
// CHECK: ;
// CHECK: ; }

cbuffer T
{
	float4 a[2];
	float4 b[2];
}
static const struct
{
	float4 a[2];
	float4 b[2];
} ST = { a, b};

uint i;

float4 main() : SV_Target
{
  return ST.a[i] + ST.b[i];
}
