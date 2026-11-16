// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -fvk-sampler-heap-stride 16 -spirv %s > %t
// RUN: FileCheck %s < %t
// RUN: FileCheck %s --check-prefix=ABSENT < %t
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-resource-heap-stride 128 -spirv %s | FileCheck %s --check-prefix=RS-ONLY
// RUN: %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -fvk-sampler-heap-stride 16  -spirv %s | FileCheck %s --check-prefix=SS-ONLY

// Verifies: -fvk-resource-heap-stride / -fvk-sampler-heap-stride emit a literal
// ArrayStride for their heap instead of ArrayStrideIdEXT. The shader loads a
// buffer and an image from the resource heap and a sampler from the sampler
// heap, and each runtime array is checked by name.
// The single-flag cases (RS-ONLY / SS-ONLY) verify that the other heap keeps its
// computed ArrayStrideIdEXT instead of taking the literal stride.

// Both flags: all three arrays carry their heap's literal stride.
// CHECK-DAG:   %[[BufType:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer{{$}}
// CHECK-DAG:   %[[ImgType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:  %[[SampType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:  %[[BufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[BufType]]{{$}}
// CHECK-DAG:  %[[ImgArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[ImgType]]{{$}}
// CHECK-DAG: %[[SampArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SampType]]{{$}}
// CHECK-DAG:                                OpDecorate %[[BufArray]] ArrayStride 128
// CHECK-DAG:                                OpDecorate %[[ImgArray]] ArrayStride 128
// CHECK-DAG:                                OpDecorate %[[SampArray]] ArrayStride 16

// No computed stride may appear anywhere in the module. This is a separate
// check, because a NOT after the last DAG match only covers the rest of the
// output, and the decorations come before the types.
// ABSENT-NOT: ArrayStrideIdEXT

// Resource heap only: the sampler heap keeps its computed stride.
// RS-ONLY-DAG:  %[[BufType:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer{{$}}
// RS-ONLY-DAG:  %[[ImgType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// RS-ONLY-DAG: %[[SampType:[a-zA-Z0-9_]+]] = OpTypeSampler
// RS-ONLY-DAG:  %[[BufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[BufType]]{{$}}
// RS-ONLY-DAG:  %[[ImgArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[ImgType]]{{$}}
// RS-ONLY-DAG: %[[SampArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SampType]]{{$}}
// RS-ONLY-DAG: OpDecorate %[[BufArray]] ArrayStride 128
// RS-ONLY-DAG: OpDecorate %[[ImgArray]] ArrayStride 128
// RS-ONLY-DAG: OpDecorateId %[[SampArray]] ArrayStrideIdEXT

// Sampler heap only: the resource heap keeps its computed stride.
// SS-ONLY-DAG:  %[[BufType:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer{{$}}
// SS-ONLY-DAG:  %[[ImgType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// SS-ONLY-DAG: %[[SampType:[a-zA-Z0-9_]+]] = OpTypeSampler
// SS-ONLY-DAG:  %[[BufArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[BufType]]{{$}}
// SS-ONLY-DAG:  %[[ImgArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[ImgType]]{{$}}
// SS-ONLY-DAG: %[[SampArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SampType]]{{$}}
// SS-ONLY-DAG: OpDecorateId %[[BufArray]] ArrayStrideIdEXT
// SS-ONLY-DAG: OpDecorateId %[[ImgArray]] ArrayStrideIdEXT
// SS-ONLY-DAG: OpDecorate %[[SampArray]] ArrayStride 16

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    StructuredBuffer<uint> sb   = ResourceDescriptorHeap[0];
    SamplerState           samp = SamplerDescriptorHeap[0];
    Texture2D<float4>      tex  = ResourceDescriptorHeap[1];
    outputBytes.Store(0, sb.Load(tid.x));
    outputBytes.Store(4, (uint)tex.SampleLevel(samp, float2(0, 0), 0).r);
}
