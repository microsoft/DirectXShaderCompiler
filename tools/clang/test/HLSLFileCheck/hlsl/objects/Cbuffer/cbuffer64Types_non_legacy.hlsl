// RUN: %dxilver 1.2 | %dxc -E main -T ps_6_2 -not_use_legacy_cbuf_load %s | FileCheck %s

// CHECK: ; cbuffer Foo
// CHECK: ; {
// CHECK: ;
// CHECK: ;   struct Foo
// CHECK: ;   {
// CHECK: ;
// CHECK: ;       double4 d;                                    ; Offset:    0
// CHECK: ;       int64_t4 i;                                   ; Offset:   32
// CHECK: ;
// CHECK: ;   } Foo;                                            ; Offset:    0 Size:    64
// CHECK: ;
// CHECK: ; }

// CHECK: %dx.types.Handle = type { i8* }
// CHECK: double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 0, i32 8)
// CHECK: double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 8, i32 8)
// CHECK: double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 16, i32 8)
// CHECK: double @dx.op.cbufferLoad.f64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 24, i32 8)
// CHECK: i64 @dx.op.cbufferLoad.i64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 32, i32 8)
// CHECK: i64 @dx.op.cbufferLoad.i64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 40, i32 8)
// CHECK: i64 @dx.op.cbufferLoad.i64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 48, i32 8)
// CHECK: i64 @dx.op.cbufferLoad.i64(i32 58, %dx.types.Handle %Foo_cbuffer, i32 56, i32 8)

cbuffer Foo {
  double4 d;
  int64_t4 i;
}

float4 main() : SV_Target {
  return d + i;
}