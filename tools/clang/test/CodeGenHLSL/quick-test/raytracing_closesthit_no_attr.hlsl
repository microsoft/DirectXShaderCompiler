// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: shader must include attributes structure parameter

struct MyPayload {
  float4 color;
  uint2 pos;
};

[shader("closesthit")]
void closesthit_no_attr( inout MyPayload payload ) {}
