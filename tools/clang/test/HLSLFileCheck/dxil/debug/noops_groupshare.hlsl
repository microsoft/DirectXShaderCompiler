// RUN: %dxc -E main -T cs_6_0 %s -Od | FileCheck %s

RWBuffer<float> uav : register(u0);

cbuffer cb : register(b0) {
  float foo;
  uint i;
}

groupshared float bar;

[numthreads(1, 1, 1)]
[RootSignature("CBV(b0), DescriptorTable(UAV(u0))")]
void main() {

  // CHECK: %[[p_load:[0-9]+]] = load i32, i32*
  // CHECK-SAME: @dx.preserve.value
  // CHECK: %[[p:[0-9]+]] = trunc i32 %[[p_load]] to i1

  // CHECK: store
  bar = 1;

  // CHECK: store
  bar = foo;

  // select i1 [[p]],
  float ret = foo;

  // select i1 [[p]],
  ret = bar;

  // CHECK: call void @dx.op.bufferStore.f32(
  uav[i] = ret;
}

