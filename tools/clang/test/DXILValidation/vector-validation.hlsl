// RUN: %dxc -T vs_6_9 %s -Od | FileCheck %s

// HLSL source for validation that vector operations produce errors pre-6.9
// See LitDxilValidation/vector-validation.ll.
// Output is modified to have 6.8 instead.

RWStructuredBuffer<float4> VecBuf;

// some simple ways to generate the vector ops in question.
// CHECK-LABEL: define void @main
float4 main(float val : VAL) :SV_Position {
  float4 vec = VecBuf[1];
  VecBuf[0] = val;
  return vec[2];
}

