// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure select resource works for lib profile.
// CHECK: call %dx.types.Handle @dx.op.createHandle(

RWStructuredBuffer<float2> buf0;
RWStructuredBuffer<float2> buf1;


void Store(bool bBufX, float2 v, uint idx) {
  RWStructuredBuffer<float2> buf = bBufX ? buf0: buf1;
  buf[idx] = v;
}