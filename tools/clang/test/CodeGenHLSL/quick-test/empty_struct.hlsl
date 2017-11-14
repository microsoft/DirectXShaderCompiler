// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Make sure nest empty struct works.
// CHECK: main

struct EmptyStruct
{
};

struct OuterStruct
{
  EmptyStruct s;
};

float4 main(float4 pos : POSITION) : SV_POSITION
{
  OuterStruct os;
  return float4(0, 0, 0, 0);
}