// RUN: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

AppendStructuredBuffer<T> myAppendStructuredBuffer[];

void main() {}

// CHECK: :8:27: error: arrays of append or consume structured buffers unsupported
