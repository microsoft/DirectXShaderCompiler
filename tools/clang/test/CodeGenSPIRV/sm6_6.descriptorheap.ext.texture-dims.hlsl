// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: sampled (non-MS, non-cube) texture dimensionalities lower 
//  to the correct OpTypeImage dim flags and feed OpImageFetch.
//
// Texture1D<float4>      -> OpTypeImage %float 1D 2 0 0 1 Unknown -> sampled
// Texture1DArray<float4> -> OpTypeImage %float 1D 2 1 0 1 Unknown -> sampled
// Texture2DArray<float4> -> OpTypeImage %float 2D 2 1 0 1 Unknown -> sampled
// Texture3D<float4>      -> OpTypeImage %float 3D 2 0 0 1 Unknown -> sampled

// CHECK-DAG:   %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:    %[[Tex1DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 2 0 0 1 Unknown
// CHECK-DAG: %[[Tex1DArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 2 1 0 1 Unknown
// CHECK-DAG: %[[Tex2DArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 1 0 1 Unknown
// CHECK-DAG:    %[[Tex3DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 3D 2 0 0 1 Unknown

// CHECK-DAG:     %[[RA_Tex1D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex1DType]]{{$}}
// CHECK-DAG:  %[[RA_Tex1DArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex1DArrType]]{{$}}
// CHECK-DAG:  %[[RA_Tex2DArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DArrType]]{{$}}
// CHECK-DAG:     %[[RA_Tex3D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex3DType]]{{$}}

// Default heap stride: OpConstantSizeOfEXT + ArrayStrideIdEXT per element type.
// CHECK-DAG:    %[[Tex1DSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Tex1DType]]
// CHECK-DAG: %[[Tex1DArrSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Tex1DArrType]]
// CHECK-DAG: %[[Tex2DArrSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Tex2DArrType]]
// CHECK-DAG:    %[[Tex3DSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[Tex3DType]]
// CHECK-DAG:                                   OpDecorateId %[[RA_Tex1D]] ArrayStrideIdEXT %[[Tex1DSize]]
// CHECK-DAG:                                   OpDecorateId %[[RA_Tex1DArr]] ArrayStrideIdEXT %[[Tex1DArrSize]]
// CHECK-DAG:                                   OpDecorateId %[[RA_Tex2DArr]] ArrayStrideIdEXT %[[Tex2DArrSize]]
// CHECK-DAG:                                   OpDecorateId %[[RA_Tex3D]] ArrayStrideIdEXT %[[Tex3DSize]]

// CHECK:     %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

RWByteAddressBuffer output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture1D<float4> tex1d = ResourceDescriptorHeap[0];
  // CHECK:       %[[T1D_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex1D]] %[[ResourceHeap]] %uint_0
  // CHECK:            %[[T1D:[a-zA-Z0-9_]+]] = OpLoad %[[Tex1DType]] %[[T1D_Desc]]

  Texture1DArray<float4> tex1dArr = ResourceDescriptorHeap[1];
  // CHECK:      %[[T1DA_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex1DArr]] %[[ResourceHeap]] %uint_1
  // CHECK:           %[[T1DA:[a-zA-Z0-9_]+]] = OpLoad %[[Tex1DArrType]] %[[T1DA_Desc]]

  Texture2DArray<float4> tex2dArr = ResourceDescriptorHeap[2];
  // CHECK:      %[[T2DA_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex2DArr]] %[[ResourceHeap]] %uint_2
  // CHECK:           %[[T2DA:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2DArrType]] %[[T2DA_Desc]]

  Texture3D<float4> tex3d = ResourceDescriptorHeap[3];
  // CHECK:       %[[T3D_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex3D]] %[[ResourceHeap]] %uint_3
  // CHECK:            %[[T3D:[a-zA-Z0-9_]+]] = OpLoad %[[Tex3DType]] %[[T3D_Desc]]

  // CHECK:                                     OpImageFetch %v4float %[[T1D]]
  float4 v = tex1d.Load(int2(tid.x, 0));
  // CHECK:                                     OpImageFetch %v4float %[[T1DA]]
  v += tex1dArr.Load(int3(tid.x, 0, 0));
  // CHECK:                                     OpImageFetch %v4float %[[T2DA]]
  v += tex2dArr.Load(int4(tid.xy, 0, 0));
  // CHECK:                                     OpImageFetch %v4float %[[T3D]]
  v += tex3d.Load(int4(tid.xyz, 0));

  output.Store(0, asuint(v.x));
}
