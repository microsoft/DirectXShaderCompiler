// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                 \
// RUN:   -fspv-target-env=vulkan1.3                                         \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                            \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                           \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                  \
// RUN:   -DOUT -spirv %s 2>&1 | FileCheck %s

// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                 \
// RUN:   -fspv-target-env=vulkan1.3                                         \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                            \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                           \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                  \
// RUN:   -DINOUT -spirv %s 2>&1 | FileCheck %s

// Verifies: a RaytracingAccelerationStructure initialized from
// ResourceDescriptorHeap is only a compile-time name for the loaded handle.
// Passing it to an out/inout parameter would drop any assignment the callee
// makes (resource parameters are not copied back), so it is rejected instead
// of silently tracing against the stale heap handle.

// CHECK: error: passing a RaytracingAccelerationStructure initialized from ResourceDescriptorHeap to an out/inout parameter is not supported with SPV_EXT_descriptor_heap
// CHECK-NOT: fatal error

RaytracingAccelerationStructure bound : register(t0);
RWBuffer<float4> output : register(u0);

#if defined(OUT)
#define QUALIFIER out
#else
#define QUALIFIER inout
#endif

void setIt(QUALIFIER RaytracingAccelerationStructure o,
           RaytracingAccelerationStructure b) {
  o = b;
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
  setIt(scene, bound);
  output[tid.x] = float4(1.0, 0.0, 0.0, 0.0);
}
