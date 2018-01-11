// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Fine.
[shader("callable")]
void callable_nop() {}

// CHECK: error: return type for ray tracing shaders must be void

struct MyParam {
  float2 coord;
  float4 output;
};

[shader("callable")]
float callable_ret( inout MyParam param ) { return 1.0; }
