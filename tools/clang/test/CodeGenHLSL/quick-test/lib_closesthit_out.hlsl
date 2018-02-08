// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: ray payload parameter must be inout
// CHECK: error: intersection attributes parameter must be in

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("closesthit")]
void closesthit_out( out MyPayload payload : SV_RayPayload,
                     out MyAttributes attr : SV_IntersectionAttributes ) {}
