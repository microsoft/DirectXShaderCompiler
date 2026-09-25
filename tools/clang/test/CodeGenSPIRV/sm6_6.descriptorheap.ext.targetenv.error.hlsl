// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.2 -spirv %s 2>&1 | FileCheck %s

// Verifies that -fspv-use-descriptor-heap rejects target environments below
// Vulkan 1.3 with a clear diagnostic.

// CHECK: error: Vulkan 1.3 is required for DescriptorHeap but not permitted to use

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture2D<float4> tex = ResourceDescriptorHeap[tid.x];
  (void)tex;
}
