// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: multisampled image types (Texture2DMS `%float 2D 2 0 1 1` and Texture2DMSArray `2D 2 1 1 1`) 
//  sourced from the descriptor heap are loaded and drive OpImageFetch %v4float.

// CHECK-DAG: %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:     %[[MSType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 1 1 Unknown
// CHECK-DAG:  %[[MSArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 1 1 1 Unknown

// CHECK-DAG:      %[[RA_MS:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[MSType]]{{$}}
// CHECK-DAG:   %[[RA_MSArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[MSArrType]]{{$}}

// CHECK:   %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

RWByteAddressBuffer output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture2DMS<float4> texMS = ResourceDescriptorHeap[0];
  // CHECK:      %[[MS_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_MS]] %[[ResourceHeap]] %uint_0
  // CHECK:           %[[MS:[a-zA-Z0-9_]+]] = OpLoad %[[MSType]] %[[MS_Desc]]

  Texture2DMSArray<float4> texMSArr = ResourceDescriptorHeap[1];
  // CHECK:   %[[MSArr_Desc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_MSArr]] %[[ResourceHeap]] %uint_1
  // CHECK:        %[[MSArr:[a-zA-Z0-9_]+]] = OpLoad %[[MSArrType]] %[[MSArr_Desc]]

  // CHECK:                                   OpImageFetch %v4float %[[MS]]
  float4 v = texMS.Load(int2(tid.xy), 0);
  // CHECK:                                   OpImageFetch %v4float %[[MSArr]]
  v += texMSArr.Load(int3(tid.xy, 0), 0);

  output.Store(0, asuint(v.x));
}
