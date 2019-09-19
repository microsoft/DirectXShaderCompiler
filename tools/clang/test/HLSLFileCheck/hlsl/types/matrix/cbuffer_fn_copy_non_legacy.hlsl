// RUN: %dxc -E VSMain -T vs_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer C
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct C
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.S
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 f4;                                ; Offset:    0
// CHECK: ;
// CHECK: ;       } s1;                                         ; Offset:    0
// CHECK: ;
// CHECK: ;       struct struct.S
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           float4 f4;                                ; Offset:   16
// CHECK: ;
// CHECK: ;       } s2;                                         ; Offset:   16
// CHECK: ;
// CHECK: ;
// CHECK: ;   } C;                                              ; Offset:    0 Size:    32
// CHECK: ;
// CHECK: ; }

// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 0, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 4, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 8, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 12, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 16, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 20, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 24, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %C_cbuffer, i32 28, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  
struct S {
  float4 f4;
  float4 get_f4() { return f4; }
};

cbuffer C {
  S s1;
  S s2;
};

struct PSInput {
 float4 position : SV_POSITION;
 float4 color : COLOR;
};

PSInput VSMain(float4 position: POSITION, float4 color: COLOR) {
 float aspect = 320.0 / 200.0;
 PSInput result;
 S s3 = s2;
 result.position = s1.get_f4();
 result.color = s2.get_f4() * s3.get_f4();
 return result;
}
