// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: BUFFER atomics on a heap-sourced RWStructuredBuffer 
//  lower through a heap access-chain + OpBufferPointerEXT into 
//  OpAtomicIAdd/OpAtomicCompareExchange on %uint.
//
// RWStructuredBuffer<uint> -> heap access-chain + OpBufferPointerEXT -> OpAtomicIAdd %uint
// RWStructuredBuffer<uint> -> heap access-chain + OpBufferPointerEXT -> OpAtomicCompareExchange %uint

// CHECK-DAG: %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:  %[[SBBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG: %[[SBBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBBufDesc]]
// CHECK:   %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWStructuredBuffer<uint> counter = ResourceDescriptorHeap[0];

  // CHECK:         %[[Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[SBBufArray]] %[[ResourceHeap]] %uint_0
  // CHECK:                                   OpBufferPointerEXT
  uint original;
  // CHECK:                                   OpAtomicIAdd %uint
  InterlockedAdd(counter[0], 1, original);

  // CHECK:                                   OpAtomicCompareExchange %uint
  uint cmp;
  InterlockedCompareExchange(counter[1], original, original + 1, cmp);
}
