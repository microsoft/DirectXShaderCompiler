// Run: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

RWStructuredBuffer<T> myRWStructuredBuffer[4];
AppendStructuredBuffer<T> myAppendStructuredBuffer[];
ConsumeStructuredBuffer<T> myConsumeStructuredBuffer[2];

void main() {}

// CHECK: :8:23: error: arrays of RW/append/consume structured buffers unsupported
// CHECK: :9:27: error: arrays of RW/append/consume structured buffers unsupported
// CHECK: :10:28: error: arrays of RW/append/consume structured buffers unsupported
