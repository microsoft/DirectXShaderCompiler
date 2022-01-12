// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// Make sure only 1 buffer store.
// CHECK:call void @dx.op.bufferStore.f32
// CHECK-NOT:call void @dx.op.bufferStore.f32

struct S {

 float a[10];
 float4 b;
};

float c;

RWStructuredBuffer<S> u;
float4 main(uint i:I) : SV_Target {
S s = u[i];
if (c > 3)
s.b += sin(s.b);
u[i] = s;
  return u[i+1].a[0];
}