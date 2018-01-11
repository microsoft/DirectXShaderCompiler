// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: intersection attributes parameter must be in

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("anyhit")]
void anyhit_inout_attr( inout MyAttributes attr : SV_IntersectionAttributes ) {}
