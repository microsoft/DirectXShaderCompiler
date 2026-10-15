// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: InterlockedAdd on a heap-sourced RWTexture2D lowers 
//  to OpUntypedImageTexelPointerEXT (not OpImageTexelPointer) 
//  feeding OpAtomicIAdd, and reassignment targets the new descriptor.
//
// RWTexture2D heap access -> OpUntypedAccessChainKHR         -> descriptor pointer in resource heap
// atomic texel pointer    -> OpUntypedImageTexelPointerEXT   -> EXT untyped image texel pointer
// InterlockedAdd          -> OpAtomicIAdd                    -> image atomic on untyped texel pointer
// reassignment            -> OpUntypedAccessChainKHR %uint_3 -> atomic uses new descriptor index

// CHECK-DAG: %[[UntypedUniformConstant:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:              %[[RWTexType:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 2 R32ui
// CHECK-DAG:             %[[RWTexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexType]]
// CHECK-DAG:           %[[UntypedImage:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR Image

// CHECK:               %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedUniformConstant]] UniformConstant

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[0];

  uint original;
  // CHECK:                     %[[Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_0
  // CHECK-NOT:                                           OpImageTexelPointer
  // CHECK:                 %[[TexelPtr:[a-zA-Z0-9_]+]] = OpUntypedImageTexelPointerEXT %[[UntypedImage]] %[[RWTexType]] %[[Desc]]
  // CHECK:                                               OpAtomicIAdd %uint %[[TexelPtr]]
  InterlockedAdd(tex[tid.xy], 1, original);

  uint directOriginal;
  // CHECK:               %[[DirectDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_1
  // CHECK-NOT:                                           OpImageTexelPointer
  // CHECK:           %[[DirectTexelPtr:[a-zA-Z0-9_]+]] = OpUntypedImageTexelPointerEXT %[[UntypedImage]] %[[RWTexType]] %[[DirectDesc]]
  // CHECK:                                               OpAtomicIAdd %uint %[[DirectTexelPtr]]
  InterlockedAdd(((RWTexture2D<uint>)ResourceDescriptorHeap[1])[tid.xy], 2,
                 directOriginal);

  // Reassignment: atomic must use the NEW descriptor (index 3, not 2).
  RWTexture2D<uint> reassigned = ResourceDescriptorHeap[2];
  reassigned = ResourceDescriptorHeap[3];
  uint reassignedOriginal;
  // CHECK:             %[[ReassignDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[RWTexArray]] %[[ResourceHeap]] %uint_3
  // CHECK-NOT:                                           OpImageTexelPointer
  // CHECK:         %[[ReassignTexelPtr:[a-zA-Z0-9_]+]] = OpUntypedImageTexelPointerEXT %[[UntypedImage]] %[[RWTexType]] %[[ReassignDesc]]
  // CHECK:                                               OpAtomicIAdd %uint %[[ReassignTexelPtr]]
  InterlockedAdd(reassigned[tid.xy], 3, reassignedOriginal);

  outputBytes.Store(0, original);
  outputBytes.Store(4, directOriginal);
  outputBytes.Store(8, reassignedOriginal);
}
