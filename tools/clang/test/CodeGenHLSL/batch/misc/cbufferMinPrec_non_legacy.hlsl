// RUN: %dxc -E main -T ps_6_0 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: Minimum-precision data types
// CHECK: ; cbuffer Foo
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct dx.alignment.legacy.Foo
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       min16float h1;                                ; Offset:    0
// CHECK: ;       float3 f3;                                    ; Offset:    4
// CHECK: ;       min16float2 h2;                               ; Offset:   16
// CHECK: ;       float3 f3_1;                                  ; Offset:   24
// CHECK: ;       float2 f2;                                    ; Offset:   36
// CHECK: ;       min16float4 h4;                               ; Offset:   44
// CHECK: ;       min16float2 h2_1;                             ; Offset:   60
// CHECK: ;       min16float3 h3;                               ; Offset:   68
// CHECK: ;       double d1;                                    ; Offset:   80
// CHECK: ;
// CHECK: ;   } Foo;                                            ; Offset:    0 Size:    88
// CHECK: ;
// CHECK: ; }

// CHECK: %dx.types.Handle = type { i8* }

// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 0, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 4, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 16, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 18, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 32, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call float @dx.op.cbufferLoad.f32(i32 58, %dx.types.Handle %Foo_cbuffer, i32 36, i32 4)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 44, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 46, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 48, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 50, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 60, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 62, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 68, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 70, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call half @dx.op.cbufferLoad.f16(i32 58, %dx.types.Handle %Foo_cbuffer, i32 72, i32 2)  ; CBufferLoad(handle,byteOffset,alignment)
// CHECK: call double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 80, i32 8)  ; CBufferLoad(handle,byteOffset,alignment)

cbuffer Foo {
  min16float h1;
  float3 f3;
  min16float2 h2;
  float3 f3_1;
  float2 f2;
  min16float4 h4;
  min16float2 h2_1;
  min16float3 h3;
  double d1;
}

float4 main() : SV_Target {
  return h1 + f3.x + h2.x + h2.y + f3_1.z + f2.x + h4.x + h4.y + h4.z + h4.w + h2_1.x + h2_1.y + h3.x + h3.y + h3.z + d1;
}