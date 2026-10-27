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
// The stride is decided in HandleTranslationUnit from the stage and the
// -fspv-extension list. Without a ray-tracing stage or a listed provider
// extension it has no acceleration structure term, so the load is rejected:
// max(sizeof(image), sizeof(buffer)) may be too narrow.
//
// A compute shader is not a ray-tracing stage. The first run lists no provider
// extension and must fail. The second lists SPV_KHR_ray_query, which this
// shader also needs for RayQuery (SPV_KHR_ray_tracing alone does not provide
// the RayQueryKHR capability), and must compile.

// CHECK: error: acceleration structure loaded from ResourceDescriptorHeap requires the resource heap stride to account for acceleration structure descriptors; compile with -fspv-extension=SPV_KHR_ray_tracing or -fspv-extension=SPV_KHR_ray_query

// OK-DAG:    %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// OK-DAG:                               OpConstantSizeOfEXT %uint %[[Accel]]

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
