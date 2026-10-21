// Verifies that the shared resource-heap array stride covers acceleration
// structure descriptors, i.e. it is
//   max(max(sizeof(image), sizeof(buffer)), sizeof(acceleration structure))
// and that every resource runtime array uses it.
//
// The stride is widened before any code is generated, for either of these
// causes. Each configuration below triggers exactly one of them:
//   - an entry point is a ray-tracing stage (lib_6_6 with RT_STAGE, using the
//     default extensions);
//   - the user listed SPV_KHR_ray_query with -fspv-extension (cs_6_6).
//
// The texture is accessed before the acceleration structure. The stride must
// already cover the acceleration structure when the texture's runtime array is
// created, otherwise that array would use a narrower stride.
//
// Without either cause the stride has only the image and buffer terms; that is
// covered by sm6_6.descriptorheap.ext.array-stride.hlsl.

// RUN: %dxc -T lib_6_6 -D RT_STAGE -fspv-use-descriptor-heap               \
// RUN:   -fspv-target-env=vulkan1.3 -spirv %s                              \
// RUN:   | FileCheck %s --check-prefixes=CHECK,RT

// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap                    \
// RUN:   -fspv-target-env=vulkan1.3                                        \
// RUN:   -fspv-extension=SPV_EXT_descriptor_heap                           \
// RUN:   -fspv-extension=SPV_KHR_untyped_pointers                          \
// RUN:   -fspv-extension=SPV_KHR_ray_query                                 \
// RUN:   -spirv %s | FileCheck %s

// RUN: %dxc -T lib_6_6 -D RT_STAGE -fspv-use-descriptor-heap               \
// RUN:   -fspv-target-env=vulkan1.3 -spirv %s                              \
// RUN:   | FileCheck %s --check-prefix=SZRT

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
// 1) ImgPlaceholder: a canonical sampled 2D float image, used only as the
//    operand of OpConstantSizeOfEXT. All image types have the same descriptor
//    size, so one placeholder stands for every image.
//
// 2) TexDesc: the type Texture2D<float4> lowers to. It is the element type of
//    the texture heap runtime array.
//
// CHECK-DAG: %[[ImgPlaceholder:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK-DAG:        %[[TexDesc:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown

// UBuf is the placeholder for the buffer descriptor size; the shader does not
// declare it.
// CHECK-DAG:           %[[UBuf:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform

// The sampler is only used by the ray-tracing stage shader.
// RT-DAG:        %[[Samp:[a-zA-Z0-9_]+]] = OpTypeSampler

// Heap runtime arrays of the accessed element types.
// CHECK-DAG:       %[[AccelArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CHECK-DAG:         %[[ImgArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// RT-DAG:     %[[SampArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Samp]]

// Resource stride = max(max(image_size, buffer_size), accel_size).
// CHECK-DAG:          %[[ImgSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[ImgPlaceholder]]
// CHECK-DAG:          %[[BufSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBuf]]
// CHECK-DAG:           %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]
// RT-DAG:      %[[SampSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Samp]]

// Three-way max: max(max(img, buf), accel).
// CHECK-DAG:             %[[IB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:          %[[MaxIB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IB]] %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:            %[[IBA:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[MaxIB]] %[[ASSz]]
// CHECK-DAG:         %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IBA]] %[[MaxIB]] %[[ASSz]]

// All resource arrays share the three-way max stride. The texture array is
// created first; if the stride had not been widened by then, its decoration
// would reference the two-way max and the first check below would fail.
// CHECK-DAG:                                     OpDecorateId %[[ImgArr]]   ArrayStrideIdEXT %[[Stride]]
// CHECK-DAG:                                     OpDecorateId %[[AccelArr]] ArrayStrideIdEXT %[[Stride]]

// The sampler heap has its own stride.
// RT-DAG:        OpDecorateId %[[SampArr]] ArrayStrideIdEXT %[[SampSz]]

// The ray-tracing stage run measures image, buffer, acceleration structure and
// sampler descriptors; the ray query run measures the first three. The checks
// above assert each of those queries, so what remains is to rule out extra
// queries, such as one per accessed element type.
//
// The lowered Texture2D type must never be measured: the image placeholder
// stands for every image descriptor.
// SZRT-DAG: %[[SZRT_Tex:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// SZRT-NOT: OpConstantSizeOfEXT %uint %[[SZRT_Tex]]
// SZRQ-DAG: %[[SZRQ_Tex:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// SZRQ-NOT: OpConstantSizeOfEXT %uint %[[SZRQ_Tex]]

// The ray query shader uses no sampler, so no sampler size query can be
// emitted for it.
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

#else // !RT_STAGE: compute shader using RayQuery

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
