// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: callable parameter must be declared inout

struct MyParam {
  float2 coord;
  float4 output;
};

[shader("callable")]
void callable_in( in MyParam param ) {}
