// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

struct T {
  float  a;
  float3 b;
};

RWStructuredBuffer<T> myRWStructuredBuffer[4];

void main() {}

// CHECK: :8:23: error: arrays of RW/append/consume structured buffers unsupported
