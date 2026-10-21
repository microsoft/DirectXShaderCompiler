// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: the shared resource-heap stride is defined even when the first
// heap access is in an unreachable block.
//
// The stride is built on the first heap access and shared by every resource
// runtime array, so its instructions must not depend on the block of that
// access. Here the first access is dead code after `return`; the second access
// is reachable and reuses the cached stride.

// CHECK-DAG: %[[ImgBigger:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan
// CHECK-DAG:   %[[ResSize:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[ImgBigger]]
// CHECK-DAG:                                OpDecorateId %{{[a-zA-Z0-9_]+}} ArrayStrideIdEXT %[[ResSize]]

RWBuffer<float4> o : register(u0);
cbuffer C : register(b0) { uint c; };

[numthreads(1, 1, 1)]
void main(uint3 t : SV_DispatchThreadID) {
  if (c == 1) {
    o[0] = 1;
    return;
    Texture2D<float4> dead = ResourceDescriptorHeap[0];
    o[1] = dead.Load(int3(0, 0, 0));
  }
  Texture2D<float4> tx = ResourceDescriptorHeap[1];
  o[t.x] = tx.Load(int3(0, 0, 0));
}
