// RUN: %dxc -E main -T ps_6_2 -enable-16bit-types -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer constants
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct constants
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       half2 h2_1;                                   ; Offset:    0
// CHECK: ;       float3 f3_1;                                  ; Offset:    4
// CHECK: ;       float3 f3_2;                                  ; Offset:   16
// CHECK: ;       half2 h2_2;                                   ; Offset:   28
// CHECK: ;
// CHECK: ;   } constants;                                      ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %constants_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %constants_cbuffer, i32 2, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %constants_cbuffer, i32 28, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %constants_cbuffer, i32 30, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %constants_cbuffer, i32 20, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)

cbuffer constants : register(b0)
{
    half2 h2_1;
    float3 f3_1;
    float3 f3_2;
    half2 h2_2;
}

float main() : SV_TARGET
{
    half res1 = h2_1[0] + h2_1[1];
    half res2 = h2_2[0] + h2_2[1];
    float f = f3_2[1];
    return res1 + res2 + f;
}
