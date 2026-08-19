// RUN: %dxc -E main -T cs_6_5 -fspv-target-env=vulkan1.2 -spirv %s | FileCheck %s

// Verify that a user-defined type named RayQuery in a namespace is NOT
// misidentified as the built-in RayQuery type. The user-defined type should
// be lowered as a plain struct, not as the OpTypeRayQueryKHR SPIR-V type.

// CHECK-NOT: OpCapability RayQueryKHR
// CHECK-NOT: OpTypeRayQueryKHR

namespace myns {
  struct RayQuery {
    float x;
    float y;
  };
}

RWStructuredBuffer<float> output : register(u0);

[numthreads(1,1,1)]
void main() {
  myns::RayQuery q;
  q.x = 1.0;
  q.y = 2.0;
  // CHECK: OpStore
  output[0] = q.x + q.y;
}
