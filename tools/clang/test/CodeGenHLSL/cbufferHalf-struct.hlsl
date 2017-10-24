// RUN: %dxc -E main -T ps_6_2 -no-min-precision %s | FileCheck %s

// CHECK: Use native low precision
// CHECK:   struct struct.Foo
// CHECK:   {
// CHECK:       half h1;                                    ; Offset:    0
// CHECK:       float3 f3;                                  ; Offset:    4

// CHECK:       half2 h2;                                   ; Offset:   16
// CHECK:       float3 f3_1;                                ; Offset:   20

// CHECK:       float2 f2;                                  ; Offset:   32
// CHECK:       half4 h4;                                   ; Offset:   40

// CHECK:       half2 h2_1;                                 ; Offset:   48
// CHECK:       half3 h3;                                   ; Offset:   52

// CHECK:       double d1;                                  ; Offset:   64
// CHECK:       half3 h3_1;                                 ; Offset:   72

// CHECK:       int i1;                                     ; Offset:   80
// CHECK:       double d2;                                  ; Offset:   88

// CHECK:   } f                                             ; Offset:    0 Size:    96

// CHECK:   struct struct.Bar
// CHECK:   {
// CHECK:       half h1;                                    ; Offset:    0
// CHECK:       half h2;                                    ; Offset:    2
// CHECK:       half h3;                                    ; Offset:    4
// CHECK:       half2 h4;                                   ; Offset:    6
// CHECK:       half3 h5;                                   ; Offset:   10

// CHECK:       half3 h7;                                   ; Offset:   16
// CHECK:       half4 h8;                                   ; Offset:   22
// CHECK:       half h9;                                    ; Offset:   30

// CHECK:       half4 h10;                                  ; Offset:   32
// CHECK:       half3 h11;                                  ; Offset:   40

// CHECK:       half2 h12;                                  ; Offset:   48
// CHECK:       half3 h13;                                  ; Offset:   52
// CHECK:       half2 h14;                                  ; Offset:   58
// CHECK:   } b                                             ; Offset:    0 Size:    62

// CHECK: %dx.types.CBufRet.f16.8 = type { half, half, half, half, half, half, half, half }

// CHECK: %f_buffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %f_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %f_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %f_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %f_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %f_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %f_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 5
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 6
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 7
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %f_buffer, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 2
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f64 @dx.op.cbufferLoadLegacy.f64(i32 59, %dx.types.Handle %f_buffer, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f64 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %f_buffer, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %f_buffer, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.i32 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f64 @dx.op.cbufferLoadLegacy.f64(i32 59, %dx.types.Handle %f_buffer, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f64 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16.8 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %b_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16.8 {{%[0-9]+}}, 0

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
  
  int   i1;
  double d2;
};

struct Bar {
  half h1;
  half h2;
  half h3;
  half2 h4;
  half3 h5;
  
  half3 h7;
  half4 h8;
  half h9;

  half4 h10;
  half3 h11;
  
  half2 h12;
  half3 h13;
  half2 h14;
};

ConstantBuffer<Foo> f : register(b0);
ConstantBuffer<Bar> b : register(b1);

float4 main() : SV_Target {
  return f.h1 + f.f3.x + f.h2.x + f.h2.y + f.f3_1.z + f.f2.x + f.h4.x + f.h4.y 
  + f.h4.z + f.h4.w + f.h2_1.x + f.h2_1.y + f.h3.x + f.h3.y + f.h3.z + f.d1 + f.h3_1.x + f.i1 + f.d2
  + b.h1;
}