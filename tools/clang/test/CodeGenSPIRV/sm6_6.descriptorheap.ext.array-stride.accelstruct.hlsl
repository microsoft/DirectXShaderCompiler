// RUN: %dxc -T lib_6_6 -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies that the shared resource-heap array stride covers acceleration
// structure descriptors, i.e. it is
//   max(max(sizeof(image), sizeof(buffer)), sizeof(acceleration structure))
// and that every resource runtime array uses it.
//
// A ray-tracing stage widens the stride with the default extensions. The stride
// is a heap layout contract decided before any code is generated, so it does not
// depend on what the shader loads. Widening from a listed extension, and the
// rejection of an acceleration structure load when the stride was not widened,
// are covered by sm6_6.descriptorheap.ext.array-stride.accelstruct-ext.hlsl and
// sm6_6.descriptorheap.ext.acceleration-structure.stride.error.hlsl. Without
// any cause the stride has only the image and buffer terms; that is covered by
// sm6_6.descriptorheap.ext.array-stride.hlsl.
//
// The shader accesses a texture, then an acceleration structure, then a
// sampler. The stride must already cover the acceleration structure when the
// texture's runtime array is created, otherwise that array would use a narrower
// stride.

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
// CHECK-DAG:           %[[Samp:[a-zA-Z0-9_]+]] = OpTypeSampler

// Heap runtime arrays of the accessed element types.
// CHECK-DAG:       %[[AccelArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Accel]]
// CHECK-DAG:         %[[ImgArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// CHECK-DAG:        %[[SampArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Samp]]

// Resource stride = max(max(image_size, buffer_size), accel_size).
// CHECK-DAG:          %[[ImgSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[ImgPlaceholder]]
// CHECK-DAG:          %[[BufSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBuf]]
// CHECK-DAG:           %[[ASSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Accel]]
// CHECK-DAG:         %[[SampSz:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Samp]]

// Three-way max: max(max(img, buf), accel).
// CHECK-DAG:             %[[IB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:          %[[MaxIB:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IB]] %[[ImgSz]] %[[BufSz]]
// CHECK-DAG:            %[[IBA:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[MaxIB]] %[[ASSz]]
// CHECK-DAG:         %[[Stride:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[IBA]] %[[MaxIB]] %[[ASSz]]

// All resource arrays share the three-way max stride. The texture array is
// created first; if the stride had not been widened by then, its decoration
// would reference the two-way max and the first check below would fail. The
// sampler heap has its own stride.
// CHECK-DAG:                                     OpDecorateId %[[ImgArr]]   ArrayStrideIdEXT %[[Stride]]
// CHECK-DAG:                                     OpDecorateId %[[AccelArr]] ArrayStrideIdEXT %[[Stride]]
// CHECK-DAG:                                     OpDecorateId %[[SampArr]]  ArrayStrideIdEXT %[[SampSz]]

// That no other size query is emitted per accessed type is checked once, in
// sm6_6.descriptorheap.ext.array-stride.hlsl, because the stride is built by
// the same cached code whatever it contains.

struct Payload   { float4 color; };
struct Attribute { float2 bary;  };

[shader("closesthit")]
void main(inout Payload payload, in Attribute attr) {
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
