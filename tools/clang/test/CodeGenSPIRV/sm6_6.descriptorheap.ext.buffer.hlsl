// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: StructuredBuffer / RWStructuredBuffer / ByteAddressBuffer 
//   share one StorageBuffer runtime array, yet each materializes its own 
//   typed OpBufferPointerEXT, ConstantBuffer uses a separate Uniform array, 
//   and reassignment re-indexes the shared array.
//
//   StructuredBuffer    -> shared OpTypeBufferEXT StorageBuffer array     -> own typed OpBufferPointerEXT (%type_StructuredBuffer_*)
//   RWStructuredBuffer  -> shared OpTypeBufferEXT StorageBuffer array     -> own typed OpBufferPointerEXT (%type_RWStructuredBuffer_*)
//   ByteAddressBuffer   -> shared OpTypeBufferEXT StorageBuffer array     -> own typed OpBufferPointerEXT (%type_ByteAddressBuffer)
//   ConstantBuffer      -> separate OpTypeBufferEXT Uniform array         -> own typed OpBufferPointerEXT (%type_ConstantBuffer_*)
//   reassignment        -> re-indexes shared StorageBuffer array (uint_4) -> same %type_StructuredBuffer_* pointer

// CHECK-DAG: %[[UntypedPtrType:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:      %[[SBBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG:     %[[SBBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBBufDesc]]
// CHECK-DAG:       %[[UBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform
// CHECK-DAG:      %[[UBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[UBufDesc]]
// Anchor each pointer type to its struct-type prefix to prevent FileCheck from
// binding all three StorageBuffer captures to the scalar %_ptr_StorageBuffer_uint.
// CHECK-DAG:     %[[SBInputPtr:[a-zA-Z0-9_]+]] = OpTypePointer StorageBuffer %type_StructuredBuffer_{{.*}}
// CHECK-DAG:    %[[SBOutputPtr:[a-zA-Z0-9_]+]] = OpTypePointer StorageBuffer %type_RWStructuredBuffer_{{.*}}
// CHECK-DAG:     %[[SBBytesPtr:[a-zA-Z0-9_]+]] = OpTypePointer StorageBuffer %type_ByteAddressBuffer{{$}}
// CHECK-DAG:      %[[UConstPtr:[a-zA-Z0-9_]+]] = OpTypePointer Uniform %type_ConstantBuffer_{{.*}}

// CHECK:       %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtrType]] UniformConstant

struct Constants {
  uint value;
};

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  StructuredBuffer<uint> input = ResourceDescriptorHeap[0];
  RWStructuredBuffer<uint> output = ResourceDescriptorHeap[1];
  ByteAddressBuffer inputBytes = ResourceDescriptorHeap[2];
  ConstantBuffer<Constants> constants = ResourceDescriptorHeap[3];

  // CHECK:        %[[InputDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[SBBufArray]] %[[ResourceHeap]] %uint_0
  // CHECK:                                       OpBufferPointerEXT %[[SBInputPtr]] %[[InputDesc]]
  // CHECK:       %[[OutputDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[SBBufArray]] %[[ResourceHeap]] %uint_1
  // CHECK:                                       OpBufferPointerEXT %[[SBOutputPtr]] %[[OutputDesc]]
  // CHECK:   %[[InputBytesDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[SBBufArray]] %[[ResourceHeap]] %uint_2
  // CHECK:                                       OpBufferPointerEXT %[[SBBytesPtr]] %[[InputBytesDesc]]
  // CHECK:    %[[ConstantsDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[UBufArray]] %[[ResourceHeap]] %uint_3
  // CHECK:                                       OpBufferPointerEXT %[[UConstPtr]] %[[ConstantsDesc]]
  output[tid.x] = input.Load(tid.x) + inputBytes.Load(tid.x * 4) + constants.value;
  outputBytes.Store(tid.x * 4, output[tid.x]);

  // Reassignment: verify new descriptor (index 4) is used after reassign.
  input = ResourceDescriptorHeap[4];
  // CHECK:   %[[ReassignedDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[SBBufArray]] %[[ResourceHeap]] %uint_4
  // CHECK:                                       OpBufferPointerEXT %[[SBInputPtr]] %[[ReassignedDesc]]
  outputBytes.Store(tid.x * 4 + 4, input.Load(tid.x));
}
