// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_query -spirv %s | FileCheck %s --check-prefixes=CHECK,RQ
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_tracing -spirv %s | FileCheck %s --check-prefixes=CHECK,KHR
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_NV_ray_tracing -spirv %s | FileCheck %s --check-prefixes=CHECK,NV

// Verifies: listing an extension that provides the acceleration structure type
// (SPV_KHR_ray_query, SPV_KHR_ray_tracing or SPV_NV_ray_tracing) with
// -fspv-extension widens the resource-heap stride to cover acceleration
// structures, even for a stage that is not a ray-tracing stage. The type is
// declared with the capability and extension of the listed extension.
//
// The full stride formula is checked in
// sm6_6.descriptorheap.ext.array-stride.accelstruct.hlsl; here it is enough that
// the acceleration structure size is the last term of the stride and that the
// texture runtime array uses the widened stride.

// RQ-DAG:                                OpCapability RayQueryKHR
// RQ-DAG:                                OpExtension "SPV_KHR_ray_query"
// KHR-DAG:                               OpCapability RayTracingKHR
// KHR-DAG:                               OpExtension "SPV_KHR_ray_tracing"
// NV-DAG:                                OpCapability RayTracingNV
// NV-DAG:                                OpExtension "SPV_NV_ray_tracing"

// CHECK-DAG: %[[TexDesc:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:  %[[ImgArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// CHECK-DAG:   %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// CHECK-DAG:    %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]
// CHECK-DAG:  %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %{{[a-zA-Z0-9_]+}} %{{[a-zA-Z0-9_]+}} %[[ASSz]]
// CHECK-DAG:                              OpDecorateId %[[ImgArr]] ArrayStrideIdEXT %[[Stride]]

RWBuffer<float4> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture2D<float4> tex = ResourceDescriptorHeap[0];
  output[tid.x] = tex.Load(int3(tid.x, 0, 0));
}
