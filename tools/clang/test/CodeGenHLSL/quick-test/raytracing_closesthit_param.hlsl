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
[shader("closesthit")]
void closesthit_nop( inout MyPayload payload, in MyAttributes attr ) {}

// CHECK: error: return type for ray tracing shaders must be void
// CHECK: error: ray payload parameter must be inout
// CHECK: error: payload and attribute structures must be user defined types with only numeric contents.
// CHECK: error: payload and attribute structures must be user defined types with only numeric contents.

[shader("closesthit")]
float closesthit_param( in bool extra, RWByteAddressBuffer buf ) { return extra.x; }
