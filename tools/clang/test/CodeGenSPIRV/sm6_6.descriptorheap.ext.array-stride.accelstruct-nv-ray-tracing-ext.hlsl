// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_NV_ray_tracing -spirv %s | FileCheck %s

// Verifies: listing SPV_NV_ray_tracing with -fspv-extension widens the
// resource-heap stride to cover acceleration structures, even for a stage that
// is not a ray-tracing stage. The acceleration structure type is declared with
// the NV capability and extension, matching the extension the user listed.

// CHECK-DAG:                             OpCapability RayTracingNV
// CHECK-DAG:                             OpExtension "SPV_NV_ray_tracing"

// CHECK-DAG:  %[[ImgSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %{{[a-zA-Z0-9_]+}}
// CHECK-DAG:  %[[BufSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %type_buffer_ext
// CHECK-DAG:   %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %accelerationStructureNV
// CHECK-DAG:     %[[IB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:  %[[MaxIB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IB]] %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:    %[[IBA:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[MaxIB]] %[[ASSz]]
// CHECK-DAG: %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IBA]] %[[MaxIB]] %[[ASSz]]
// CHECK-DAG:                             OpDecorateId %{{[a-zA-Z0-9_]+}} ArrayStrideIdEXT %[[Stride]]

RWBuffer<float4> o : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 t : SV_DispatchThreadID) {
  Texture2D<float4> tx = ResourceDescriptorHeap[1];
  o[t.x] = tx.Load(int3(0, 0, 0));
}
