// Run: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

ConsumeStructuredBuffer<T> myConsumeStructuredBuffer[2];

void main() {}

// CHECK: :8:28: error: arrays of RW/append/consume structured buffers unsupported
