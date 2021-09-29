// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Make sure only 1 bufferLoad at offset 640.

// CHECK:call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle
// CHECK-SAME:, i32 640)
// CHECK-NOT:call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32

struct S {

 float4x4 a[10];
 float4 b;
};

RWStructuredBuffer<S> u;


void foo(inout S s) {
  s.b += sin(s.b);
}

float4 main(uint i:I) : SV_Target {
  foo(u[i]);
  return u[i].b;
}