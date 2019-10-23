// RUN: %dxc -E main -T ps_6_2 -enable-16bit-types -not_use_legacy_cbuf_load %s | FileCheck %s

// Github issue: #972

// CHECK: ; cbuffer constants
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct constants
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.CData
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           half4 a;                                  ; Offset:    0
// CHECK: ;           half4 b;                                  ; Offset:    8
// CHECK: ;           half4 c;                                  ; Offset:   16
// CHECK: ;           half4 d;                                  ; Offset:   24
// CHECK: ;
// CHECK: ;       } constants;                                  ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } constants;                                      ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 0
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 4
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 6
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 8
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 10
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 12
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 14
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 16
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 18
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 20
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 22
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 24
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 26
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 28
// CHECK: @dx.op.cbufferLoad.f16
// CHECK: i32 30

#define TYPE half4

struct CData
{
    TYPE a;
    TYPE b;
    TYPE c;
    TYPE d;
};

ConstantBuffer<CData> constants;

struct VStoPS
{
    TYPE position : SV_Position;
};

TYPE main(VStoPS input) : SV_Target
{
    return (constants.a * constants.b + constants.c * constants.d);
}