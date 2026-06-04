// RUN: %dxc -T lib_6_6 -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 64 -fvk-sampler-heap-stride 32 -fspv-extension=SPV_KHR_ray_tracing -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -spirv %s | FileCheck %s

// Verifies: ResourceDescriptorHeap of a RaytracingAccelerationStructure 
//  lowers to an acceleration-structure runtime array, loads the accel 
//  handle from the heap, and emits OpTraceRayKHR under the ray-tracing 
//  capability/extension.

// CHECK:                                                 OpCapability RayTracingKHR
// CHECK:                                                 OpExtension "SPV_KHR_ray_tracing"

// CHECK-DAG: %[[UntypedUniformConstant:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:                  %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// CHECK-DAG:                %[[ASArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CHECK:               %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedUniformConstant]] UniformConstant

struct Payload {
  float4 color;
};

struct Attribute {
  float2 bary;
};

[shader("closesthit")]
void main(inout Payload payload, in Attribute attr) {
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[3];

  RayDesc ray;
  ray.Origin = float3(0.0f, 0.0f, 0.0f);
  ray.Direction = float3(0.0f, 0.0f, -1.0f);
  ray.TMin = 0.0f;
  ray.TMax = 1000.0f;

  Payload childPayload = { float4(attr.bary, 0.0f, 1.0f) };

  // CHECK:                     %[[Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[ASArray]] %[[ResourceHeap]] %uint_3
  // CHECK:                    %[[Scene:[a-zA-Z0-9_]+]] = OpLoad %[[Accel]] %[[Desc]]
  // CHECK:                                               OpTraceRayKHR %[[Scene]]
  TraceRay(scene, 0x0, 0xff, 0, 1, 0, ray, childPayload);

  payload.color = childPayload.color;
}
