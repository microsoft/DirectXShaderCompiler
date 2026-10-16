// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_NV_ray_tracing -spirv %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_NV_ray_tracing -spirv %s | FileCheck %s --check-prefix=NOAS

// Verifies: listing only SPV_NV_ray_tracing does not widen the resource-heap
// stride for a non-ray-tracing stage.
//
// Widening adds an acceleration structure type to the module for the size
// query. Nothing declares a capability that makes that type valid in a
// non-ray-tracing stage when only SPV_NV_ray_tracing is listed, so the module
// would be invalid even though the shader never uses an acceleration structure.

// NOAS-NOT: OpTypeAccelerationStructure
// NOAS-NOT: OpCapability RayTracing
// NOAS-NOT: OpCapability RayQuery

// CHECK-DAG: %[[ImgBigger:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan
// CHECK-DAG: %[[ResSize:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[ImgBigger]]
// CHECK-DAG: OpDecorateId %{{[a-zA-Z0-9_]+}} ArrayStrideIdEXT %[[ResSize]]

RWBuffer<float4> o : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 t : SV_DispatchThreadID) {
  Texture2D<float4> tx = ResourceDescriptorHeap[1];
  o[t.x] = tx.Load(int3(0, 0, 0));
}
