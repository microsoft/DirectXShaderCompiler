// RUN: %dxc -T cs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// In a compound assignment, a default argument that reads a texel is evaluated
// for each call: counter should be incremented twice.

RWTexture2D<float4> tex : register(u0);
RWStructuredBuffer<float> output : register(u1);
static uint counter = 0;

float readNext(float x = tex[uint2(counter++, 0)].x) { return x; }

[numthreads(1, 1, 1)]
void main() {
  float sum = 0;
// CHECK:      [[n1:%[0-9]+]] = OpLoad %uint %counter
// CHECK-NEXT: [[n1n:%[0-9]+]] = OpIAdd %uint [[n1]] %uint_1
// CHECK-NEXT: OpStore %counter [[n1n]]
// CHECK:      OpFunctionCall %float %readNext
// CHECK:      [[n2:%[0-9]+]] = OpLoad %uint %counter
// CHECK-NEXT: [[n2n:%[0-9]+]] = OpIAdd %uint [[n2]] %uint_1
// CHECK-NEXT: OpStore %counter [[n2n]]
// CHECK:      OpFunctionCall %float %readNext
  sum += readNext() + readNext();
  output[0] = sum;
}
