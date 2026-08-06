// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: storage RW texture dimensionality lowers to distinct 
//  OpTypeImage dim/flags, each driving OpImageRead/OpImageWrite.
//
// RWTexture1D<float4>      -> OpTypeImage %float 1D 2 0 0 2 Rgba32f -> storage
// RWTexture1DArray<float4> -> OpTypeImage %float 1D 2 1 0 2 Rgba32f -> storage
// RWTexture2DArray<float4> -> OpTypeImage %float 2D 2 1 0 2 Rgba32f -> storage
// RWTexture3D<float4>      -> OpTypeImage %float 3D 2 0 0 2 Rgba32f -> storage

// CHECK-DAG:  %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:    %[[RW1DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 2 0 0 2 Rgba32f
// CHECK-DAG: %[[RW1DArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 2 1 0 2 Rgba32f
// CHECK-DAG: %[[RW2DArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 1 0 2 Rgba32f
// CHECK-DAG:    %[[RW3DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 3D 2 0 0 2 Rgba32f

// CHECK-DAG:     %[[RA_RW1D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RW1DType]]{{$}}
// CHECK-DAG:  %[[RA_RW1DArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RW1DArrType]]{{$}}
// CHECK-DAG:  %[[RA_RW2DArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RW2DArrType]]{{$}}
// CHECK-DAG:     %[[RA_RW3D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RW3DType]]{{$}}

// All RWTextures are resource descriptors, so every array shares one resource stride 
// (derivation covered by sm6_6.descriptorheap.ext.array-stride.hlsl)
// CHECK-DAG: OpDecorateId %[[RA_RW1D]] ArrayStrideIdEXT %[[ResSize:[a-zA-Z0-9_]+]]
// CHECK-DAG: OpDecorateId %[[RA_RW1DArr]] ArrayStrideIdEXT %[[ResSize]]
// CHECK-DAG: OpDecorateId %[[RA_RW2DArr]] ArrayStrideIdEXT %[[ResSize]]
// CHECK-DAG: OpDecorateId %[[RA_RW3D]] ArrayStrideIdEXT %[[ResSize]]

// CHECK:    %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture1D<float4> rwTex1d = ResourceDescriptorHeap[0];
  // CHECK:     %[[RW1D_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RW1D]] %[[ResourceHeap]] %uint_0
  // CHECK:          %[[RW1D:[a-zA-Z0-9_]+]] = OpLoad %[[RW1DType]] %[[RW1D_Desc]]

  RWTexture1DArray<float4> rwTex1dArr = ResourceDescriptorHeap[1];
  // CHECK:    %[[RW1DA_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RW1DArr]] %[[ResourceHeap]] %uint_1
  // CHECK:         %[[RW1DA:[a-zA-Z0-9_]+]] = OpLoad %[[RW1DArrType]] %[[RW1DA_Desc]]

  RWTexture2DArray<float4> rwTex2dArr = ResourceDescriptorHeap[2];
  // CHECK:    %[[RW2DA_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RW2DArr]] %[[ResourceHeap]] %uint_2
  // CHECK:         %[[RW2DA:[a-zA-Z0-9_]+]] = OpLoad %[[RW2DArrType]] %[[RW2DA_Desc]]

  RWTexture3D<float4> rwTex3d = ResourceDescriptorHeap[3];
  // CHECK:     %[[RW3D_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RW3D]] %[[ResourceHeap]] %uint_3
  // CHECK:          %[[RW3D:[a-zA-Z0-9_]+]] = OpLoad %[[RW3DType]] %[[RW3D_Desc]]

  // CHECK:                                    OpImageRead %v4float %[[RW1D]]
  float4 v = rwTex1d[tid.x];

  // CHECK:                                    OpImageWrite %[[RW1DA]]
  rwTex1dArr[uint2(tid.x, 0)] = v;

  // CHECK:                                    OpImageWrite %[[RW2DA]]
  rwTex2dArr[uint3(tid.xy, 0)] = v;

  // CHECK:                                    OpImageWrite %[[RW3D]]
  rwTex3d[tid.xyz] = v;
}
