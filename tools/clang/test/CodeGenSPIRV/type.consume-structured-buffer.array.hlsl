// RUN: %dxc -T ps_6_0 -E main -fvk-allow-rwstructuredbuffer-arrays

struct T {
  float  a;
  float3 b;
};


// CHECK: %myConsumeStructuredBuffer = OpVariable %_ptr_Uniform__arr_type_ConsumeStructuredBuffer_T_uint_2 Uniform
ConsumeStructuredBuffer<T> myConsumeStructuredBuffer[2];

void main() {}

