// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_INCREMENT %s 2>&1 | FileCheck --check-prefix=INC %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=DEC %s

// Verifies: IncrementCounter/DecrementCounter on a 
//  heap-loaded RWStructuredBuffer is rejected with a hard error under 
//  SPV_EXT_descriptor_heap.

// INC: counter operations on heap-loaded RWStructuredBuffer are not supported with SPV_EXT_descriptor_heap
// DEC: counter operations on heap-loaded RWStructuredBuffer are not supported with SPV_EXT_descriptor_heap

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main() {
  RWStructuredBuffer<uint> buffer = ResourceDescriptorHeap[0];

#ifdef TEST_INCREMENT
  uint value = buffer.IncrementCounter();
#else
  uint value = buffer.DecrementCounter();
#endif

  outputBytes.Store(0, value);
}
