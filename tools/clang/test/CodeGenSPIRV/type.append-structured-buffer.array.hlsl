// RUN: %dxc -T ps_6_0 -E main

struct T {
  float  a;
  float3 b;
};

// CHECK: %myAppendStructuredBuffer = OpVariable %_ptr_Uniform__runtimearr_type_AppendStructuredBuffer_T Uniform
AppendStructuredBuffer<T> myAppendStructuredBuffer[];

void main() {}

