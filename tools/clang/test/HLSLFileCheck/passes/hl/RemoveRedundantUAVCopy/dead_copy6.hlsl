// RUN: %dxc -T ps_6_0 -E main -opt-enable hl-remove-redundant-uav-ldst  %s | FileCheck %s

// Make sure only 1 buffer store.
// CHECK:call void @dx.op.bufferStore.f32
// CHECK:call void @dx.op.bufferStore.f32

struct S {

 float4x4 a[10];
 float4 b;
};

RWStructuredBuffer<S> u;

uint idx;
void foo(inout S s) {

  s.b += sin(s.b);
  s.a[idx][0] += 1;
}

float4 main(uint i:I) : SV_Target {
  foo(u[i]);
  return u[i].b;
}