// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: only one parameter (ray payload) allowed for miss shader

struct MyPayload {
  float4 color;
  uint2 pos;
};

[shader("miss")]
void miss_extra( inout MyPayload payload : SV_RayPayload,
                 float extra) {}
