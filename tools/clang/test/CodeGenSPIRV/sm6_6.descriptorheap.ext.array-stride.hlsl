// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s --check-prefix=DEDUP

// Verifies the default descriptor-heap array stride.
//
// With no manual intervention, every heap runtime array must be decorated with ArrayStrideIdEXT 
// referencing an OpConstantSizeOfEXT of the array's element (descriptor) type:
//
//   StructuredBuffer / RWStructuredBuffer / ByteAddressBuffer -> OpTypeBufferEXT StorageBuffer
//   ConstantBuffer                                            -> OpTypeBufferEXT Uniform
//   Texture2D                                                 -> OpTypeImage
//   SamplerState                                              -> OpTypeSampler
//
// The OpConstantSizeOfEXT result type is %uint

// CHECK-DAG: %[[UntypedPtrType:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant

// Element (descriptor) types.
// CHECK-DAG:      %[[SBBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG:       %[[UBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform
// CHECK-DAG:        %[[TexDesc:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D
// CHECK-DAG:    %[[SamplerDesc:[a-zA-Z0-9_]+]] = OpTypeSampler

// Heap runtime arrays of those element types.
// CHECK-DAG:     %[[SBBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBBufDesc]]
// CHECK-DAG:      %[[UBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[UBufDesc]]
// CHECK-DAG:       %[[TexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// CHECK-DAG:   %[[SamplerArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerDesc]]

// One OpConstantSizeOfEXT per distinct element type, result type %uint.
// CHECK-DAG:      %[[SBBufSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[SBBufDesc]]
// CHECK-DAG:       %[[UBufSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBufDesc]]
// CHECK-DAG:        %[[TexSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[TexDesc]]
// CHECK-DAG:    %[[SamplerSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[SamplerDesc]]

// Each heap array is decorated ArrayStrideIdEXT with its element's sizeof.
// CHECK-DAG:                                     OpDecorateId %[[SBBufArray]] ArrayStrideIdEXT %[[SBBufSize]]
// CHECK-DAG:                                     OpDecorateId %[[UBufArray]] ArrayStrideIdEXT %[[UBufSize]]
// CHECK-DAG:                                     OpDecorateId %[[TexArray]] ArrayStrideIdEXT %[[TexSize]]
// CHECK-DAG:                                     OpDecorateId %[[SamplerArray]] ArrayStrideIdEXT %[[SamplerSize]]

// No literal default stride decoration on any heap array.
// CHECK-NOT:                                     OpDecorate %{{.*}} ArrayStride 64
// CHECK-NOT:                                     OpDecorate %{{.*}} ArrayStride 32

// Dedup: StructuredBuffer and ByteAddressBuffer below both lower to the same
// OpTypeBufferEXT StorageBuffer descriptor, so its OpConstantSizeOfEXT must be
// emitted exactly ONCE and shared, not one per heap access. 

// Separate prefix: annotations precede the constants section in SPIR-V, so a
// trailing ordered check in the main pass would search past the sizeof.
// {{$}} anchors the match: "StorageBuffer" is a prefix of "StorageBuffer_0"
// (the Uniform descriptor name), so without it DEDUP-NOT would false-fire.

// DEDUP:             %[[SBDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// DEDUP-COUNT-1:                                 OpConstantSizeOfEXT %uint %[[SBDesc]]{{$}}
// DEDUP-NOT:                                     OpConstantSizeOfEXT %uint %[[SBDesc]]{{$}}

struct Constants {
  uint value;
};

RWStructuredBuffer<float4> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  StructuredBuffer<uint> sbuf = ResourceDescriptorHeap[0];
  ByteAddressBuffer babuf = ResourceDescriptorHeap[1];
  ConstantBuffer<Constants> cbuf = ResourceDescriptorHeap[2];
  Texture2D<float4> tex = ResourceDescriptorHeap[3];
  SamplerState samp = SamplerDescriptorHeap[0];

  float4 color = tex.SampleLevel(samp, float2(0, 0), 0);
  output[tid.x] = color + sbuf.Load(tid.x) + babuf.Load(tid.x * 4) + cbuf.value;
}
