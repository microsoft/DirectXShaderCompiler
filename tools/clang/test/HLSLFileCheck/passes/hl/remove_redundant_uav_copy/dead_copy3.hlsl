// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s

// Make sure 11 buffer store.
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32
// CHECK-NOT:call void @dx.op.bufferStore.f32


struct S {

 float a[10];
 float4 b;
};

float c;

RWStructuredBuffer<S> u[2];

uint c0;
uint c1;

[shader("pixel")]
float4 main(uint i:I) : SV_Target {
S s = u[c0][i];
if (c > 3)
s.b += sin(s.b);
u[c1][i] = s;
  return u[c1][i].b;
}