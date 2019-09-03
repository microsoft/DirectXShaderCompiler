// RUN: %dxilver 1.2 | %dxc -E main -T ps_6_2 -enable-16bit-types -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: Use native low precision
// CHECK: ; cbuffer Foo
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct Foo
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       half f_h1;                                    ; Offset:    0
// CHECK: ;       float3 f_f3;                                  ; Offset:    2
// CHECK: ;       half2 f_h2;                                   ; Offset:   14
// CHECK: ;       float3 f_f3_1;                                ; Offset:   18
// CHECK: ;       float2 f_f2;                                  ; Offset:   30
// CHECK: ;       half4 f_h4;                                   ; Offset:   38
// CHECK: ;       half2 f_h2_1;                                 ; Offset:   46
// CHECK: ;       half3 f_h3;                                   ; Offset:   50
// CHECK: ;       double f_d1;                                  ; Offset:   56
// CHECK: ;       half3 f_h3_1;                                 ; Offset:   64
// CHECK: ;       int f_i1;                                     ; Offset:   70
// CHECK: ;       double f_d2;                                  ; Offset:   74
// CHECK: ;
// CHECK: ;   } Foo;                                            ; Offset:    0 Size:    82
// CHECK: ;
// CHECK: ; }

cbuffer Foo {
  half f_h1;
  float3 f_f3;

  half2 f_h2;
  float3 f_f3_1;

  float2 f_f2;
  half4 f_h4;

  half2 f_h2_1;
  half3 f_h3;

  double f_d1;
  half3 f_h3_1;

  int    f_i1;
  double f_d2;
}

// CHECK: ; cbuffer Bar
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct Bar
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       half b_h1;                                    ; Offset:    0
// CHECK: ;       half b_h2;                                    ; Offset:    2
// CHECK: ;       half b_h3;                                    ; Offset:    4
// CHECK: ;       half2 b_h4;                                   ; Offset:    6
// CHECK: ;       half3 b_h5;                                   ; Offset:   10
// CHECK: ;       half3 b_h6;                                   ; Offset:   16
// CHECK: ;       half4 b_h7;                                   ; Offset:   22
// CHECK: ;       half b_h8;                                    ; Offset:   30
// CHECK: ;       half4 b_h9;                                   ; Offset:   32
// CHECK: ;       half3 b_h10;                                  ; Offset:   40
// CHECK: ;       half2 b_h11;                                  ; Offset:   46
// CHECK: ;       half3 b_h12;                                  ; Offset:   50
// CHECK: ;       half2 b_h13;                                  ; Offset:   56
// CHECK: ;       half b_h14;                                   ; Offset:   60
// CHECK: ;       half b_h16;                                   ; Offset:   62
// CHECK: ;       half b_h17;                                   ; Offset:   64
// CHECK: ;       half b_h18;                                   ; Offset:   66
// CHECK: ;       half b_h19;                                   ; Offset:   68
// CHECK: ;       half b_h20;                                   ; Offset:   70
// CHECK: ;       half b_h21;                                   ; Offset:   72
// CHECK: ;       half b_h22;                                   ; Offset:   74
// CHECK: ;       half b_h23;                                   ; Offset:   76
// CHECK: ;
// CHECK: ;   } Bar;                                            ; Offset:    0 Size:    78
// CHECK: ;
// CHECK: ; }

cbuffer Bar {
  half b_h1;
  half b_h2;
  half b_h3;
  half2 b_h4;
  half3 b_h5;

  half3 b_h6;
  half4 b_h7;
  half b_h8;

  half4 b_h9;
  half3 b_h10;

  half2 b_h11;
  half3 b_h12;
  half2 b_h13;
  half  b_h14;

  half b_h16;
  half b_h17;
  half b_h18;
  half b_h19;
  half b_h20;
  half b_h21;
  half b_h22;
  half b_h23;

}

// CHECK: %dx.types.Handle = type { i8* }

float4 main() : SV_Target  {
  return f_h1 + f_f3.x
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 2, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f_h2.x + f_h2.y + f_f3_1.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 26, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  + f_f2.x + f_h4.x + f_h4.y + f_h4.z + f_h4.w
  // CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 30, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f_h2_1.x + f_h2_1.y + f_h3.x + f_h3.y + f_h3.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f_d1 + f_h3_1.x
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 56, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + f_i1 + f_d2
  // CHECK: call i32 @dx.op.cbufferLoad.i32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 70, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 74, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)
  + b_h1 + b_h2 + b_h3 + b_h4.x + b_h5.y + b_h5.x + b_h5.y + b_h5.z +
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 2, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 4, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 6, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 12, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 10, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 14, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b_h6.x + b_h6.y + b_h6.z + b_h7.x + b_h7.y + b_h7.z + b_h7.w + b_h8
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 18, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 20, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 22, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 24, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 26, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 28, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 30, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b_h9.x + b_h9.y + b_h9.z + b_h9.w + b_h10.x + b_h10.y + b_h10.z
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 32, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 34, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 36, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 38, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 40, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 42, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b_h11.x + b_h11.y + b_h12.x + b_h12.y + b_h12.z + b_h13.x + b_h13.y + b_h14
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 52, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 54, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 56, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 58, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 60, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  + b_h16 + b_h17 + b_h18 + b_h19 + b_h20 + b_h21 + b_h22 + b_h23;
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 62, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 64, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 66, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 68, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 70, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 72, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 74, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
  // CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Bar_cbuffer, i32 76, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
}