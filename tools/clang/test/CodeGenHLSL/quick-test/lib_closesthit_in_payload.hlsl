// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: ray payload parameter must be inout

struct MyPayload {
  float4 color;
  uint2 pos;
};

[shader("closesthit")]
void closesthit_in_payload( in MyPayload payload : SV_RayPayload ) {}
