// RUN: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

RWStructuredBuffer<T> myRWStructuredBuffer[4];

void main() {
  myRWStructuredBuffer[0].IncrementCounter();
}

// CHECK: :11:3: fatal error: cannot find the associated counter variable
