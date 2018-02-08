// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: only one ray payload parameter allowed
// CHECK: error: semantic index must be 0
// CHECK: error: only one intersection attributes parameter allowed
// CHECK: error: semantic index must be 0

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("closesthit")]
void closesthit_2_payload_attr( inout MyPayload payload : SV_RayPayload,
                                inout MyPayload payload2 : SV_RayPayload2,
                                in MyAttributes attr : SV_IntersectionAttributes,
                                in MyAttributes attr2 : SV_IntersectionAttributes2 ) {}
