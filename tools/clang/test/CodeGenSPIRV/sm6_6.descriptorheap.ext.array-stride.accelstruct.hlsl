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
// RUN:   -spirv %s | FileCheck %s --check-prefix=SZRQ

// --- Types (user-declared + stride-computation placeholders) ---------------
// CHECK-DAG:    %[[Accel:[a-zA-Z0-9_]+]] = OpTypeAccelerationStructureKHR
// CHECK-DAG:      %[[Img:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown

// UBuf is not user-declared; it is the canonical placeholder injected by
// getResourceHeapArrayStride() for the buffer descriptor size.
// CHECK-DAG:     %[[UBuf:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform

// Sampler only present in path A (closesthit).
// RT-DAG:        %[[Samp:[a-zA-Z0-9_]+]] = OpTypeSampler

// --- Runtime array types ---------------------------------------------------
// CHECK-DAG: %[[AccelArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CHECK-DAG:   %[[ImgArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Img]]

// RT-DAG:     %[[SampArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Samp]]

// --- Size operands ---------------------------------------------------------
// CHECK-DAG:    %[[ImgSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Img]]
// CHECK-DAG:    %[[BufSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBuf]]
// CHECK-DAG:     %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]
// RT-DAG:      %[[SampSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Samp]]

// --- Three-way max: max(max(img, buf), accel) identical in both paths ----
// CHECK-DAG:       %[[IB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSz]] %[[BufSz]]     
// CHECK-DAG:    %[[MaxIB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IB]] %[[ImgSz]] %[[BufSz]]   
// CHECK-DAG:      %[[IBA:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[MaxIB]] %[[ASSz]]      
// CHECK-DAG:   %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IBA]] %[[MaxIB]] %[[ASSz]]   

// --- All resource arrays share the three-way-max stride -------------------
// %[[ImgArr]] is created FIRST (texture before AS in source); most important.
// If the stride cache were populated before noteResourceHeapHasAccelStruct()
// was called, ImgArr would be decorated with a two-way max stride, which
// would not match %[[Stride]] (bound to the three-way max above), failing here.
// CHECK-DAG:     OpDecorateId %[[ImgArr]]   ArrayStrideIdEXT %[[Stride]]       
// CHECK-DAG:     OpDecorateId %[[AccelArr]] ArrayStrideIdEXT %[[Stride]]       

// --- Sampler stride is independent (path A only) --------------------------
// RT-DAG:        OpDecorateId %[[SampArr]] ArrayStrideIdEXT %[[SampSz]]

// --- Exact OpConstantSizeOfEXT counts -------------------------------------
// Path A: img, buf, accel, sampler = 4
// SZRT-COUNT-4:  OpConstantSizeOfEXT %uint
// SZRT-NOT:      OpConstantSizeOfEXT %uint

// Path B: img, buf, accel = 3  (no sampler)
// SZRQ-COUNT-3:  OpConstantSizeOfEXT %uint
// SZRQ-NOT:      OpConstantSizeOfEXT %uint

#ifdef RT_STAGE

struct Payload   { float4 color; };
struct Attribute { float2 bary;  };

[shader("closesthit")]
void main(inout Payload payload, in Attribute attr) {
  // Ordering stress test: access texture first
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
  // Ordering stress test: access texture first
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