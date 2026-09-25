// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: an RWByteAddressBuffer sourced from the descriptor heap 
//  produces the typed StorageBuffer pointer %type_RWByteAddressBuffer 
//  and a read-WRITE OpBufferPointerEXT through it.

// CHECK-DAG: %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:  %[[SBBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG: %[[SBBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBBufDesc]]
// CHECK-DAG:   %[[RWBABPtr:[a-zA-Z0-9_]+]] = OpTypePointer StorageBuffer %type_RWByteAddressBuffer
// CHECK:   %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWByteAddressBuffer buf = ResourceDescriptorHeap[0];

  // CHECK:         %[[Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[SBBufArray]] %[[ResourceHeap]] %uint_0
  // CHECK:                                   OpBufferPointerEXT %[[RWBABPtr]] %[[Desc]]
  uint val = buf.Load(tid.x * 4);
  buf.Store(tid.x * 4 + 256, val + 1);
}
