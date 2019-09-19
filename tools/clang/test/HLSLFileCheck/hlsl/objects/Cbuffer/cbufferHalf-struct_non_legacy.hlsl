// RUN: %dxilver 1.2 | %dxc -E main -T ps_6_2 -enable-16bit-types -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: Use native low precision
// CHECK: ; cbuffer f
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct f
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Foo
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           half h1;                                  ; Offset:    0
// CHECK: ;           float3 f3;                                ; Offset:    2
// CHECK: ;           half2 h2;                                 ; Offset:   14
// CHECK: ;           float3 f3_1;                              ; Offset:   18
// CHECK: ;           float2 f2;                                ; Offset:   30
// CHECK: ;           half4 h4;                                 ; Offset:   38
// CHECK: ;           half2 h2_1;                               ; Offset:   46
// CHECK: ;           half3 h3;                                 ; Offset:   50
// CHECK: ;           double d1;                                ; Offset:   56
// CHECK: ;           half3 h3_1;                               ; Offset:   64
// CHECK: ;           int i1;                                   ; Offset:   70
// CHECK: ;           double d2;                                ; Offset:   74
// CHECK: ;
// CHECK: ;       } f;                                          ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } f;                                              ; Offset:    0 Size:    82
// CHECK: ;
// CHECK: ; }

struct Foo {
  half h1;
  float3 f3;

  half2 h2;
  float3 f3_1;

  float2 f2;
  half4 h4;

  half2 h2_1;
  half3 h3;

  double d1;
  half3 h3_1;

  int i1;
  double d2;
};


// CHECK: ; cbuffer b
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct b
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       struct struct.Bar
// CHECK: ;       {
// CHECK: ;
// CHECK: ;           half h1;                                  ; Offset:    0
// CHECK: ;           half h2;                                  ; Offset:    2
// CHECK: ;           half h3;                                  ; Offset:    4
// CHECK: ;           half2 h4;                                 ; Offset:    6
// CHECK: ;           half3 h5;                                 ; Offset:   10
// CHECK: ;           half3 h6;                                 ; Offset:   16
// CHECK: ;           half4 h7;                                 ; Offset:   22
// CHECK: ;           half h8;                                  ; Offset:   30
// CHECK: ;           half4 h9;                                 ; Offset:   32
// CHECK: ;           half3 h10;                                ; Offset:   40
// CHECK: ;           half2 h11;                                ; Offset:   46
// CHECK: ;           half3 h12;                                ; Offset:   50
// CHECK: ;           half2 h13;                                ; Offset:   56
// CHECK: ;           half h14;                                 ; Offset:   60
// CHECK: ;           half h16;                                 ; Offset:   62
// CHECK: ;           half h17;                                 ; Offset:   64
// CHECK: ;           half h18;                                 ; Offset:   66
// CHECK: ;           half h19;                                 ; Offset:   68
// CHECK: ;           half h20;                                 ; Offset:   70
// CHECK: ;           half h21;                                 ; Offset:   72
// CHECK: ;           half h22;                                 ; Offset:   74
// CHECK: ;           half h23;                                 ; Offset:   76
// CHECK: ;
// CHECK: ;       } b;                                          ; Offset:    0
// CHECK: ;
// CHECK: ;
// CHECK: ;   } b;                                              ; Offset:    0 Size:    78
// CHECK: ;
// CHECK: ; }

struct Bar {
  half h1;
  half h2;
  half h3;
  half2 h4;
  half3 h5;

  half3 h6;
  half4 h7;
  half h8;

  half4 h9;
  half3 h10;

  half2 h11;
  half3 h12;
  half2 h13;
  half  h14;

  half h16;
  half h17;
  half h18;
  half h19;
  half h20;
  half h21;
  half h22;
  half h23;

};

ConstantBuffer<Foo> f : register(b0);
ConstantBuffer<Bar> b : register(b1);

// CHECK: %dx.types.Handle = type { i8* }

float4 main() : SV_Target  {
  return f.h1 + f.f3.x
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %f_cbuffer, i32 2, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.h2.x + f.h2.y + f.f3_1.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %f_cbuffer, i32 26, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.f2.x + f.h4.x + f.h4.y + f.h4.z + f.h4.w
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %f_cbuffer, i32 30, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.h2_1.x + f.h2_1.y + f.h3.x + f.h3.y + f.h3.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.d1 + f.h3_1.x
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %f_cbuffer, i32 56, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %f_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f.i1 + f.d2
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %f_cbuffer, i32 70, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %f_cbuffer, i32 74, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h1 + b.h2 + b.h3 + b.h4.x + b.h5.y + b.h5.x + b.h5.y + b.h5.z +
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 2, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 4, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 6, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 12, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 10, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h6.x + b.h6.y + b.h6.z + b.h7.x + b.h7.y + b.h7.z + b.h7.w + b.h8
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 18, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 20, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 22, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 24, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 26, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 28, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 30, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h9.x + b.h9.y + b.h9.z + b.h9.w + b.h10.x + b.h10.y + b.h10.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 32, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 34, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 36, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h11.x + b.h11.y + b.h12.x + b.h12.y + b.h12.z + b.h13.x + b.h13.y + b.h14
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 56, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 58, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 60, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b.h16 + b.h17 + b.h18 + b.h19 + b.h20 + b.h21 + b.h22 + b.h23;
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 62, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 66, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 68, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 70, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 72, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 74, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %b_cbuffer, i32 76, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
}