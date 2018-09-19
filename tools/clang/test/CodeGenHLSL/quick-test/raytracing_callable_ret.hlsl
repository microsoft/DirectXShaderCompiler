// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

struct MyParam {
  float2 coord;
  float4 output;
};

// Fine.
[shader("callable")]
void callable_nop( inout MyParam param ) {}

// CHECK: error: return type for ray tracing shaders must be void

[shader("callable")]
float callable_ret( inout MyParam param ) { return 1.0; }
