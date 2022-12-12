// RUN: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

RWStructuredBuffer<T> myRWStructuredBuffer[4];

void main() {
  RWStructuredBuffer<T> localSBuffer = myRWStructuredBuffer[0];
  localSBuffer.IncrementCounter();
}

// CHECK: :12:3: fatal error: cannot find the associated counter variable
