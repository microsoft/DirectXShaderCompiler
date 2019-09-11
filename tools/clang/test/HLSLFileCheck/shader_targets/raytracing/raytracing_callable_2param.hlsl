// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: only one parameter allowed for callable shader

struct MyParam {
  float2 coord;
  float4 output;
};

[shader("callable")]
void callable_2param( inout MyParam param,
                      inout MyParam param2 ) {}
