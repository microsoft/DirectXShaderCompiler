// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// Make sure only 1 buffer store.
// CHECK:call void @dx.op.bufferStore.f32
// CHECK-NOT:call void @dx.op.bufferStore.f32

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