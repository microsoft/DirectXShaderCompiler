// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: error: ray payload parameter must be a user defined type with only numeric contents.

struct MyPayload {
  float4 color;
  uint2 pos;
};

[shader("miss")]
void miss_udt( inout PointStream<MyPayload> payload ) {}
