// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s --check-prefix=COUNT

// Verifies the default descriptor-heap array stride.
//
// The HLSL SM6.6 ResourceDescriptorHeap is a single flat array in which the
// client may place any resource descriptor at any slot. To match DX12
// semantics, all resource descriptor arrays must share one stride equal to
// the largest resource descriptor size: max(sizeof(image), sizeof(buffer)).
// Images and buffers are the two resource descriptor categories defined by
// VkPhysicalDeviceDescriptorHeapPropertiesEXT (imageDescriptorSize /
// bufferDescriptorSize); textures lower to OpTypeImage, so image/buffer
// covers all relevant HLSL resource kinds.
// Both sizes are driver defined and known only at pipeline creation time, so
// the maximum is computed with OpSpecConstantOp over two OpConstantSizeOfEXT
// placeholders.
//
// The sampler heap holds a single descriptor type, so its stride is simply the
// sampler descriptor size.
//
// OpConstantSizeOfEXT result type is %uint.

// CHECK-DAG: %[[UntypedPtrType:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant

// Element (descriptor) types.
// CHECK-DAG:      %[[SBBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG:       %[[UBufDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform
// CHECK-DAG:    %[[SamplerDesc:[a-zA-Z0-9_]+]] = OpTypeSampler

// Image descriptor type. Texture2D<float4> lowers to this same type
// (Dim2D, depth=0, not-arrayed, non-MS, sampled, Unknown format), so
// %[[TexDesc]] serves as both the placeholder for the size calculation and
// the real descriptor type for the Texture2D access below.
// CHECK-DAG:        %[[TexDesc:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown

// Heap runtime arrays of the accessed element types.
// CHECK-DAG:     %[[SBBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SBBufDesc]]
// CHECK-DAG:      %[[UBufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[UBufDesc]]
// CHECK-DAG:       %[[TexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexDesc]]
// CHECK-DAG:   %[[SamplerArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerDesc]]

// Resource stride = max(image_size, buffer_size); sampler stride = sampler_size.
// CHECK-DAG:        %[[ImgSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[TexDesc]]
// CHECK-DAG:        %[[BufSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[UBufDesc]]
// CHECK-DAG:      %[[ImgBigger:[a-zA-Z0-9_]+]] = OpSpecConstantOp %bool UGreaterThan %[[ImgSize]] %[[BufSize]]
// CHECK-DAG:        %[[ResSize:[a-zA-Z0-9_]+]] = OpSpecConstantOp %uint Select %[[ImgBigger]] %[[ImgSize]] %[[BufSize]]
// CHECK-DAG:       %[[SampSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[SamplerDesc]]

// Every resource runtime array shares the one resource stride; the sampler
// array uses the sampler size.
// CHECK-DAG:                                     OpDecorateId %[[SBBufArray]] ArrayStrideIdEXT %[[ResSize]]
// CHECK-DAG:                                     OpDecorateId %[[UBufArray]] ArrayStrideIdEXT %[[ResSize]]
// CHECK-DAG:                                     OpDecorateId %[[TexArray]] ArrayStrideIdEXT %[[ResSize]]
// CHECK-DAG:                                     OpDecorateId %[[SamplerArray]] ArrayStrideIdEXT %[[SampSize]]

// Three distinct descriptor heap sizes exist:
// the image/Texture2D type (%[[TexDesc]]), the Uniform buffer, and the sampler.
// Regression test: make sure of no per-element nor per acces behavior exists
// otherwise StorageBuffer would add a fourth, pushing the count above three.
// COUNT-COUNT-3: OpConstantSizeOfEXT %uint
// COUNT-NOT:     OpConstantSizeOfEXT %uint

struct Constants {
  uint value;
};

RWStructuredBuffer<float4> output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  StructuredBuffer<uint> sbuf = ResourceDescriptorHeap[0];
  ByteAddressBuffer babuf = ResourceDescriptorHeap[1];
  Texture2D<float4> tex = ResourceDescriptorHeap[2];
  ConstantBuffer<Constants> cbuf = ResourceDescriptorHeap[3];
  ConstantBuffer<Constants> cbuf2 = ResourceDescriptorHeap[3];
  
  SamplerState samp = SamplerDescriptorHeap[0];

  float4 color = tex.SampleLevel(samp, float2(0, 0), 0);
  output[tid.x] = color + sbuf.Load(tid.x) + babuf.Load(tid.x * 4) + cbuf.value + cbuf2.value;
}
