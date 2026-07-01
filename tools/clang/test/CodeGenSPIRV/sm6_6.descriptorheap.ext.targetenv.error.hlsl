// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.2 -spirv %s 2>&1 | FileCheck %s

// Verifies: using -fspv-use-descriptor-heap without -fspv-target-env=vulkan1.3
// emits an error diagnostic and does not produce SPIR-V output.

// CHECK: error: Vulkan 1.3 is required for DescriptorHeap but not permitted to use
// CHECK: note: please specify your target environment via command line option -fspv-target-env=

[numthreads(1, 1, 1)]
void main() {
  StructuredBuffer<uint> b = ResourceDescriptorHeap[0];
}
