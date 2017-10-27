// RUN: %dxc -E main -T ps_6_2 -no-min-precision %s | FileCheck %s

// CHECK: Use native low precision
// CHECK: cbuffer Foo
// CHECK: {
// CHECK:   struct Foo
// CHECK:   {
// CHECK:       half f_h1;                                    ; Offset:    0
// CHECK:       float3 f_f3;                                  ; Offset:    4

// CHECK:       half2 f_h2;                                   ; Offset:   16
// CHECK:       float3 f_f3_1;                                ; Offset:   20

// CHECK:       float2 f_f2;                                  ; Offset:   32
// CHECK:       half4 f_h4;                                   ; Offset:   40

// CHECK:       half2 f_h2_1;                                 ; Offset:   48
// CHECK:       half3 f_h3;                                   ; Offset:   52

// CHECK:       double f_d1;                                  ; Offset:   64
// CHECK:       half3 f_h3_1;                                 ; Offset:   72

// CHECK:       int f_i1;                                     ; Offset:   80
// CHECK:       double f_d2;                                  ; Offset:   88
// CHECK:   } Foo                                             ; Offset:    0 Size:    96
// CHECK: }

// CHECK: cbuffer Bar
// CHECK: {
// CHECK:   struct Bar
// CHECK:   {
// CHECK:       half b_h1;                                    ; Offset:    0
// CHECK:       half b_h2;                                    ; Offset:    2
// CHECK:       half b_h3;                                    ; Offset:    4
// CHECK:       half2 b_h4;                                   ; Offset:    6
// CHECK:       half3 b_h5;                                   ; Offset:   10

// CHECK:       half3 b_h7;                                   ; Offset:   16
// CHECK:       half4 b_h8;                                   ; Offset:   22
// CHECK:       half b_h9;                                    ; Offset:   30

// CHECK:       half4 b_h10;                                  ; Offset:   32
// CHECK:       half3 b_h11;                                  ; Offset:   40

// CHECK:       half2 b_h12;                                  ; Offset:   48
// CHECK:       half3 b_h13;                                  ; Offset:   52
// CHECK:       half2 b_h14;                                  ; Offset:   58
// CHECK:   } Bar                                             ; Offset:    0 Size:    62
// CHECK: }

// CHECK: %dx.types.CBufRet.f16.8 = type { half, half, half, half, half, half, half, half }

// CHECK: %Foo_buffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 5
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 6
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 7
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 2
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f64 @dx.op.cbufferLoadLegacy.f64(i32 59, %dx.types.Handle %Foo_buffer, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f64 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %Foo_buffer, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.i32 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f64 @dx.op.cbufferLoadLegacy.f64(i32 59, %dx.types.Handle %Foo_buffer, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f64 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Bar_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0

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
  int   f_i1;
  double f_d2;
}

cbuffer Bar {
  half b_h1;
  half b_h2;
  half b_h3;
  half2 b_h4;
  half3 b_h5;
  
  half3 b_h7;
  half4 b_h8;
  half b_h9;

  half4 b_h10;
  half3 b_h11;
  
  half2 b_h12;
  half3 b_h13;
  half2 b_h14;
}

float4 main() : SV_Target {
  return f_h1 + f_f3.x + f_h2.x + f_h2.y + f_f3_1.z + f_f2.x + f_h4.x + f_h4.y 
  + f_h4.z + f_h4.w + f_h2_1.x + f_h2_1.y + f_h3.x + f_h3.y + f_h3.z + f_d1 + f_h3_1.x + f_i1 + f_d2
  + b_h1;
}