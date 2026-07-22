// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                 \
// RUN:   -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                     \
// RUN:   -fspv-target-env=vulkan1.3                                         \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                            \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                           \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                  \
// RUN:   -spirv %s | FileCheck %s --check-prefix=OK

// Verifies: an acceleration structure loaded from ResourceDescriptorHeap is
// rejected when the resource-heap stride was not widened to include the
// acceleration structure descriptor size.
//
// The stride is decided before the code-gen loop (see HandleTranslationUnit)
// and frozen on its first use, so it cannot be widened once an acceleration
// structure heap access is reached. Without the widening this shader would
// silently get max(sizeof(image), sizeof(buffer)), which may be too narrow.
//
// A compute shader is not a ray-tracing stage, so the widening only happens
// when the user lists a ray-tracing/ray-query extension explicitly. The first
// run does not, and must fail; the second one does, and must compile.

// CHECK: error: acceleration structure loaded from ResourceDescriptorHeap requires the resource heap stride to account for acceleration structure descriptors; compile with -fspv-extension=SPV_KHR_ray_tracing or -fspv-extension=SPV_KHR_ray_query

// OK-DAG:   %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// OK-DAG: %[[AccelSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]

RWBuffer<float4> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];

  RayDesc ray;
  ray.Origin    = float3(0.0, 0.0, 0.0);
  ray.Direction = float3(0.0, 0.0, 1.0);
  ray.TMin = 0.0;
  ray.TMax = 1000.0;

  RayQuery<RAY_FLAG_NONE> q;
  q.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
  bool hit = q.Proceed();

  output[tid.x] = float4(hit ? 1.0 : 0.0, 0.0, 0.0, 0.0);
}
