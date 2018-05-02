// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: callable parameter must be declared inout

struct MyParam {
  float2 coord;
  float4 output;
};

[shader("callable")]
void callable_out( out MyParam param ) { param = (MyParam)0; }
