// Run: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

StructuredBuffer<T> myStructuredBuffer[3];
RWStructuredBuffer<T> myRWStructuredBuffer[4];
AppendStructuredBuffer<T> myAppendStructuredBuffer[2];
ConsumeStructuredBuffer<T> myConsumeStructuredBuffer[2];
ByteAddressBuffer myBAB[2];
RWByteAddressBuffer myRWBAB[2];

void main() {}

// CHECK: :8:21: error: arrays of structured/byte buffers unsupported
// CHECK: :9:23: error: arrays of structured/byte buffers unsupported
// CHECK: :10:27: error: arrays of structured/byte buffers unsupported
// CHECK: :11:28: error: arrays of structured/byte buffers unsupported
// CHECK: :12:19: error: arrays of structured/byte buffers unsupported
// CHECK: :13:21: error: arrays of structured/byte buffers unsupported
