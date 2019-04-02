// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: ray payload parameter must be inout

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("closesthit")]
void closesthit_in_payload( in MyPayload payload, MyAttributes attr ) {}
