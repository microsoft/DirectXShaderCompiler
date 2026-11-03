// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                 \
// RUN:   -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                     \
// RUN:   -fspv-target-env=vulkan1.3                                         \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                            \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                           \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                  \
// RUN:   -spirv %s | FileCheck %s --check-prefix=OK

// Verify -fvk-resource-heap-stride suppresses the diagnostic: the stride is a
// user-supplied literal, so there is no computed stride to widen. The
// acceleration structure runtime array must carry a literal ArrayStride (not
// ArrayStrideIdEXT) and no OpConstantSizeOfEXT may be emitted for the
// acceleration structure type.
// There is deliberately no -fspv-extension list: SPV_KHR_ray_query would make
// the pre-scan widen the stride by itself, so compiling would not depend on the
// override. The extensions this shader needs are enabled by default.
// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                     \
// RUN:   -fspv-target-env=vulkan1.3                                         \
// RUN:   -fvk-resource-heap-stride 256                                      \
// RUN:   -spirv %s > %t
// RUN: FileCheck %s --check-prefix=CLI < %t
// RUN: FileCheck %s --check-prefix=ABSENT < %t

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

// CHECK: error: acceleration structure loaded from ResourceDescriptorHeap requires the resource heap stride to account for acceleration structure descriptors; compile with -fspv-extension=SPV_KHR_ray_tracing, -fspv-extension=SPV_NV_ray_tracing, -fspv-extension=SPV_KHR_ray_query, or -fvk-resource-heap-stride

// The variable initialized from the rejected load must not cascade into a
// fatal error that hides the diagnostic above.
// CHECK-NOT: unregistered decl
// CHECK-NOT: fatal error

// OK-DAG:    %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// OK-DAG:                               OpConstantSizeOfEXT %uint %[[Accel]]

// The acceleration structure runtime array carries the literal stride.
// CLI-DAG:     %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// CLI-DAG:  %[[AccelArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CLI-DAG:                                OpDecorate %[[AccelArr]] ArrayStride 256

// No computed stride may appear anywhere in the module. This is a separate
// check, because a NOT after the last DAG match only covers the rest of the
// output, and the decorations come before the types.
// ABSENT-NOT: ArrayStrideIdEXT
// ABSENT-NOT: OpConstantSizeOfEXT

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
