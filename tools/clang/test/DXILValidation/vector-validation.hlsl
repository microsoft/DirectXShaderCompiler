// RUN: %dxc -T vs_6_9 %s -Od | FileCheck %s

// Just HLSL source for validation that vector operations produce errors pre-6.9
// Output is modified to have 6.8 instead.

struct Vector { int i; float4 f;};

RWStructuredBuffer<float4> VecBuf;
RWStructuredBuffer<Vector> StrBuf;
RWStructuredBuffer<float> ScalBuf;

// some simple ways to generate the vector ops in question.
// CHECK-LABEL: define void @main
float4 main(float val : VAL) :SV_Position {
  float4 vec = VecBuf[1];
  VecBuf[0] = val;
  return vec[2];
}

