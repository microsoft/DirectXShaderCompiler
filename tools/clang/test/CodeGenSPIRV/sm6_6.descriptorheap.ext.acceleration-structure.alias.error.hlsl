// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_query -spirv -DCASE=0 %s 2>&1 | FileCheck %s --check-prefix=REASSIGN
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_query -spirv -DCASE=1 %s 2>&1 | FileCheck %s --check-prefix=REASSIGN
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_query -spirv -DCASE=2 %s 2>&1 | FileCheck %s --check-prefix=REASSIGN
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fspv-extension=SPV_EXT_descriptor_heap -fspv-extension=SPV_KHR_untyped_pointers -fspv-extension=SPV_KHR_ray_query -spirv -DCASE=3 %s 2>&1 | FileCheck %s --check-prefix=MIX

// Verifies: a RaytracingAccelerationStructure initialized from
// ResourceDescriptorHeap is a compile-time name for the loaded handle, so an
// assignment that would change it is diagnosed, without crashing the compiler.
//
// CASE=0 and CASE=1 assign a bound acceleration structure to a heap-initialized
// variable, once as a statement and once as a value. CASE=2 reassigns it from
// another heap access. All three get the reassignment error, which does not
// mention bound resources. CASE=3 assigns a heap acceleration structure to a
// variable that was assigned a bound one before, which is the mixing error.

// MIX: error: mixing bound and descriptor heap resources in the same variable is not supported
// MIX-NOT: Internal compiler error
// REASSIGN: error: reassigning a RaytracingAccelerationStructure initialized from ResourceDescriptorHeap is not supported
// REASSIGN-NOT: Internal compiler error

RaytracingAccelerationStructure bound : register(t0);
RWBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RayDesc ray;
  ray.Origin = float3(0.0, 0.0, 0.0);
  ray.Direction = float3(0.0, 0.0, 1.0);
  ray.TMin = 0.0;
  ray.TMax = 1000.0;
  RayQuery<RAY_FLAG_NONE> q;
#if CASE == 0
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
  scene = bound;
  q.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
#elif CASE == 1
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
  RaytracingAccelerationStructure other = (scene = bound);
  q.TraceRayInline(other, RAY_FLAG_NONE, 0xff, ray);
#elif CASE == 2
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
  scene = ResourceDescriptorHeap[2];
  q.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
#elif CASE == 3
  RaytracingAccelerationStructure scene;
  scene = bound;
  scene = ResourceDescriptorHeap[1];
  q.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
#endif
  output[tid.x] = q.Proceed() ? 1.0 : 0.0;
}
