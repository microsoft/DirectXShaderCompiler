// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_APPEND %s 2>&1 | FileCheck --check-prefix=APPEND %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=CONSUME %s

// Verifies: Append/Consume structured buffers via ResourceDescriptorHeap 
//  emit a hard error under SPV_EXT_descriptor_heap.

// APPEND: append/consume structured buffers are not supported with SPV_EXT_descriptor_heap
// CONSUME: append/consume structured buffers are not supported with SPV_EXT_descriptor_heap

[numthreads(1, 1, 1)]
void main() {
#ifdef TEST_APPEND
  AppendStructuredBuffer<uint> output = ResourceDescriptorHeap[0];
  output.Append(1);
#else
  ConsumeStructuredBuffer<uint> input = ResourceDescriptorHeap[0];
  uint val = input.Consume();
#endif
}
