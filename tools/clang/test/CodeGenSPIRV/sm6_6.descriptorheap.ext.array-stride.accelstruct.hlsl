// Two test paths share this file; select via -D RT_STAGE:
//
//   Path A; Condition 1 (RT stage):
//     -T lib_6_6 -D RT_STAGE -fspv-extension=SPV_KHR_ray_tracing
//     A closesthit entry point is in the workQueue; shaderModelKindIsRayTracing()
//     returns true and noteResourceHeapHasAccelStruct() is called unconditionally.
//
//   Path B; Condition 3 (explicit KHR_ray_query extension):
//     -T cs_6_6 -E main -fspv-extension=SPV_KHR_ray_query
//     No RT stage in workQueue, so Condition 1 is skipped. The
//     !spirvOptions.allowedExtensions.empty() guard passes (user listed an
//     explicit extension), isExtensionEnabled(KHR_ray_query) is true, and
//     noteResourceHeapHasAccelStruct() is called.
//
// Both paths verify that the shared resource-heap array stride expands to
//   max(max(sizeof(image), sizeof(buffer)), sizeof(accel_struct))
// and that ALL resource runtime arrays share that three-way max stride.
//
// Ordering stress test: Texture2D is accessed BEFORE the AS in source order.
// The stride cache must already hold sizeof(accel_struct) when the first
// runtime array type is created so the texture array uses the correct stride.
//
// The regression test for the "default-extension-mode" false positive (no
// -fspv-extension flags -> allowedExtensions is empty -> Condition 3 is skipped)
// is sm6_6.descriptorheap.ext.array-stride.hlsl, which must emit exactly 3
// OpConstantSizeOfEXT (img, buf, sampler - no accel_struct).

// RUN: %dxc -T lib_6_6 -D RT_STAGE -fspv-use-descriptor-heap               \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_tracing                               \
// RUN:   -spirv %s | FileCheck %s --check-prefixes=CHECK,RT

// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                    \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                 \
// RUN:   -spirv %s | FileCheck %s

// RUN: %dxc -T lib_6_6 -D RT_STAGE -fspv-use-descriptor-heap               \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_tracing                               \
// RUN:   -spirv %s | FileCheck %s --check-prefix=SZRT

// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                    \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                 \
// RUN:   -spirv %s | FileCheck %s --check-prefixes=SZRQ,NOSAMP

// Element (descriptor) types.
// CHECK-DAG:          %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR

// Two distinct OpTypeImage types appear in the output:
//
// 1) ImgPlaceholder: a canonical sampled 2D float image (depth=0) used only as
//    the operand to OpConstantSizeOfEXT. All image subtypes report the same
//    imageDescriptorSize, so the specific depth field is irrelevant to the query.
//
// 2) TexDesc: the actual lowered type for Texture2D<float4> (depth=2,
//    WithDepth::Unknown), produced by LowerTypeVisitor for sampled textures.
//    This is the element type of the texture heap runtime array.
//
// CHECK-DAG: %[[ImgPlaceholder:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK-DAG:        %[[TexDesc:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown

// UBuf is the canonical buffer placeholder injected for the buffer descriptor
// size query; it is not user-declared.
// CHECK-DAG:           %[[UBuf:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform

// Sampler only present in path A (RT stage).
// RT-DAG:        %[[Samp:[a-zA-Z0-9_]+]] = OpTypeSampler

// Heap runtime arrays of the accessed element types.
// CHECK-DAG:       %[[AccelArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CHECK-DAG:         %[[ImgArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// RT-DAG:     %[[SampArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Samp]]

// Resource stride = max(max(image_size, buffer_size), accel_size).
// The size query uses ImgPlaceholder (depth=0); the driver returns the same
// imageDescriptorSize regardless of which image subtype is used as the operand.
// CHECK-DAG:          %[[ImgSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[ImgPlaceholder]]
// CHECK-DAG:          %[[BufSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBuf]]
// CHECK-DAG:           %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]
// RT-DAG:      %[[SampSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Samp]]

// Three-way max: max(max(img, buf), accel), identical in both paths.
// CHECK-DAG:             %[[IB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:          %[[MaxIB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IB]] %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:            %[[IBA:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[MaxIB]] %[[ASSz]]
// CHECK-DAG:         %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IBA]] %[[MaxIB]] %[[ASSz]]

// All resource arrays share the three-way-max stride.
// %[[ImgArr]] is created FIRST (texture before AS in source order); if the
// stride were not yet widened at array-type creation time, this decoration
// would reference the narrower two-way max and the check below would fail.
// CHECK-DAG:                                     OpDecorateId %[[ImgArr]]   ArrayStrideIdEXT %[[Stride]]
// CHECK-DAG:                                     OpDecorateId %[[AccelArr]] ArrayStrideIdEXT %[[Stride]]

// Sampler stride is independent (path A only).
// RT-DAG:        OpDecorateId %[[SampArr]] ArrayStrideIdEXT %[[SampSz]]

// Path A (RT stage) queries img, buf, accel and sampler; path B (ray query)
// queries img, buf and accel.  Each expected query is asserted by a CHECK-DAG
// above, so what remains is to rule out the extra queries that a per-element or
// per-access implementation would emit.
//
// In both paths the lowered Texture2D type must never be measured, because one
// canonical image placeholder covers every image descriptor.
// SZRT-DAG: %[[SZRT_Tex:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// SZRT-NOT: OpConstantSizeOfEXT %uint %[[SZRT_Tex]]
// SZRQ-DAG: %[[SZRQ_Tex:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// SZRQ-NOT: OpConstantSizeOfEXT %uint %[[SZRQ_Tex]]

// Path B declares no sampler, so no sampler descriptor type reaches the module
// and therefore no sampler size query can be emitted.
// NOSAMP-NOT: OpTypeSampler

#ifdef RT_STAGE

struct Payload   { float4 color; };
struct Attribute { float2 bary;  };

[shader("closesthit")]
void main(inout Payload payload, in Attribute attr) {
  // Ordering stress test: access texture first.
  Texture2D<float4>               tex   = ResourceDescriptorHeap[0];
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
  SamplerState                    samp  = SamplerDescriptorHeap[0];

  float4 color = tex.SampleLevel(samp, float2(0.0, 0.0), 0.0);
  payload.color = color;

  RayDesc ray;
  ray.Origin    = float3(0.0, 0.0,  0.0);
  ray.Direction = float3(0.0, 0.0, -1.0);
  ray.TMin = 0.0;
  ray.TMax = 1000.0;

  Payload child = { color };
  TraceRay(scene, 0x0, 0xff, 0, 1, 0, ray, child);
}

#else // !RT_STAGE, compute shader with RayQuery (path B)

RWBuffer<float4> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // Ordering stress test: access texture first.
  Texture2D<float4>               tex   = ResourceDescriptorHeap[0];
  RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];

  RayDesc ray;
  ray.Origin    = float3(0.0, 0.0, 0.0);
  ray.Direction = float3(0.0, 0.0, 1.0);
  ray.TMin = 0.0;
  ray.TMax = 1000.0;

  RayQuery<RAY_FLAG_NONE> q;
  q.TraceRayInline(scene, RAY_FLAG_NONE, 0xff, ray);
  bool hit = q.Proceed();

  int3 coord = int3(tid.x, 0, 0);
  output[tid.x] = tex.Load(coord) + float4(hit ? 1.0 : 0.0, 0.0, 0.0, 0.0);
}

#endif // RT_STAGE
