// Verifies the descriptor-heap array stride when a RaytracingAccelerationStructure
// is present in the resource heap.
//
// When acceleration structures are accessed from ResourceDescriptorHeap, the
// stride formula expands from max(sizeof(image), sizeof(buffer)) to the
// three-way max: max(max(sizeof(image), sizeof(buffer)), sizeof(accel_struct)).
// All resource runtime arrays must share this wider stride so that any slot
// can hold any descriptor type.
//
// The compiler widens the stride under two conditions:
//
//   Path A (RT stage): the shader model is a ray-tracing stage, so any heap
//   access may load an acceleration structure. Ray-tracing extensions are
//   requested explicitly via -fspv-extension=SPV_KHR_ray_tracing.
//
//   Path B (ray query): the SPV_KHR_ray_query extension is explicitly requested
//   by the user, signalling that AS descriptors may appear in the heap even
//   without a ray-tracing stage.
//
// Ordering stress test: Texture2D is accessed BEFORE the AS in source order.
// If the stride cache were populated before the AS widens it, the texture
// runtime array would be decorated with the narrower two-way max stride.
//
// For the no-AS baseline (exactly 3 OpConstantSizeOfEXT: img, buf, sampler)
// see sm6_6.descriptorheap.ext.array-stride.hlsl.

// RUN: %dxc -T lib_6_6 -D RT_STAGE -Od -fspv-use-descriptor-heap           \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_tracing                               \
// RUN:   -spirv %s | FileCheck %s --check-prefixes=CHECK,RT

// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap                \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                 \
// RUN:   -spirv %s | FileCheck %s

// RUN: %dxc -T lib_6_6 -D RT_STAGE -Od -fspv-use-descriptor-heap           \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_tracing                               \
// RUN:   -spirv %s | FileCheck %s --check-prefix=SZRT

// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap                \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                 \
// RUN:   -spirv %s | FileCheck %s --check-prefix=SZRQ

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

// Exact OpConstantSizeOfEXT counts.
// Path A (RT stage): img, buf, accel, sampler = 4.
// SZRT-COUNT-4:  OpConstantSizeOfEXT %uint
// SZRT-NOT:      OpConstantSizeOfEXT %uint

// Path B (ray query): img, buf, accel = 3 (no sampler).
// SZRQ-COUNT-3:  OpConstantSizeOfEXT %uint
// SZRQ-NOT:      OpConstantSizeOfEXT %uint

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
