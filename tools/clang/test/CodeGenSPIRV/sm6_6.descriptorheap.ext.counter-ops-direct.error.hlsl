// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_INCREMENT %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Verifies: counter ops on a direct-cast heap buffer are rejected with the same
// diagnostic as the alias form in counter-ops.error.hlsl.
//
// The direct form has no alias VarDecl, so the descriptorHeapBufferAliasVars
// lookup misses and the call used to fall through to the unrelated "Cannot
// access associated counter variable for an array of buffers in a struct"
// fatal error.

// CHECK: counter operations on heap-loaded RWStructuredBuffer are not supported with SPV_EXT_descriptor_heap

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main() {
#ifdef TEST_INCREMENT
  uint value =
      ((RWStructuredBuffer<uint>)ResourceDescriptorHeap[0]).IncrementCounter();
#else
  uint value =
      ((RWStructuredBuffer<uint>)ResourceDescriptorHeap[0]).DecrementCounter();
#endif

  outputBytes.Store(0, value);
}
