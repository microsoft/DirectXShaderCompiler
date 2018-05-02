// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: ray payload parameter must be declared inout

struct MyPayload {
  float4 color;
  uint2 pos;
};

[shader("miss")]
void miss_out( out MyPayload payload : SV_RayPayload ) { payload = (MyPayload)0; }
