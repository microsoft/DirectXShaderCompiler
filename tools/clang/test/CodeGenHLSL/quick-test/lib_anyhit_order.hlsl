// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: ray payload must be before intersection attributes

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("anyhit")]
void anyhit_order( in MyAttributes attr : SV_IntersectionAttributes,
                   inout MyPayload payload : SV_RayPayload ) {}

