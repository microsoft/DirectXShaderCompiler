// RUN: %dxilver 1.5 | %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer $Globals
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct $Globals
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.A
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 x[2];                              ; Offset:    0
// CHECK: ;           uint y[1];                                ; Offset:   32
// CHECK: ;
// CHECK: ;       } a[2];;                                      ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } $Globals;                                       ; Offset:    0 Size:    72
// CHECK: ;
// CHECK: ; }
// CHECK: ;
// CHECK: ; cbuffer cb
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct cb
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       float b;                                      ; Offset:    0
// CHECK: ;       float c[1];                                   ; Offset:    4
// CHECK: ;
// CHECK: ;   } cb;                                             ; Offset:    0 Size:     8
// CHECK: ;
// CHECK: ; }

struct A {
  float4 x[2];
  uint   y[1];
};

A a[2];

cbuffer cb {
  float b;
  float c[1];
}

float main() : SV_Target {
   // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %"$Globals_cbuffer", i32 32, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
   // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %cb_cbuffer, i32 4, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
   return a[0].y[0] + c[0];
}