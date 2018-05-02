// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

// Fine.
[shader("anyhit")]
void anyhit_nop( inout MyPayload payload, MyAttributes attr ) {}

// CHECK: error: return type for ray tracing shaders must be void
// CHECK: error: ray payload parameter must be inout
// CHECK: error: payload and attribute structures must be user defined types with only numeric contents.
// CHECK: error: payload and attribute structures must be user defined types with only numeric contents.

[shader("anyhit")]
float anyhit_param( in float4 extra, Texture2D tex0 ) { return extra.x; }
