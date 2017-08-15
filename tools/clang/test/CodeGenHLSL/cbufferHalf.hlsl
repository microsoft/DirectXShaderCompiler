// RUN: %dxc -E main -T ps_6_0 -no-min-precision %s | FileCheck %s

// CHECK: cbuffer Foo
// CHECK: {
// CHECK:   struct dx.alignment.legacy.Foo
// CHECK:   {
// CHECK:       min16f h1;                                    ; Offset:    0
// CHECK:       float3 f3;                                    ; Offset:    4
// CHECK:       min16f2 h2;                                   ; Offset:   16
// CHECK:       float3 f3_1;                                  ; Offset:   20
// CHECK:       float2 f2;                                    ; Offset:   32
// CHECK:       min16f4 h4;                                   ; Offset:   40
// CHECK:       min16f2 h2_1;                                 ; Offset:   48
// CHECK:       min16f3 h3;                                   ; Offset:   52
// CHECK:       double d1;                                    ; Offset:   64
// CHECK:   } Foo                                             ; Offset:    0 Size:    72
// CHECK: }

// CHECK: %Foo_buffer = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f32 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 5
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 6
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 7
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f16 @dx.op.cbufferLoadLegacy.f16(i32 59, %dx.types.Handle %Foo_buffer, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 0
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 1
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 2
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 3
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f16 {{%[0-9]+}}, 4
// CHECK: {{%[0-9]+}} = call %dx.types.CBufRet.f64 @dx.op.cbufferLoadLegacy.f64(i32 59, %dx.types.Handle %Foo_buffer, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
// CHECK: {{%[0-9]+}} = extractvalue %dx.types.CBufRet.f64 {{%[0-9]+}}, 0

cbuffer Foo {
  half h1;
  float3 f3;
  half2 h2;
  float3 f3_1;
  float2 f2;
  half4 h4;
  half2 h2_1;
  half3 h3;
  double d1;
}

float4 main() : SV_Target {
  return h1 + f3.x + h2.x + h2.y + f3_1.z + f2.x + h4.x + h4.y + h4.z + h4.w + h2_1.x + h2_1.y + h3.x + h3.y + h3.z + d1;
}}