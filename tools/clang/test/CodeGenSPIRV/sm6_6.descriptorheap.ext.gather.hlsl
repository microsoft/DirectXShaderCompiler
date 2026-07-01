// RUN: %dxc -T ps_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: GatherRed/Green/Blue/Alpha each emit a fresh OpSampledImage that
//  reuses the once-loaded heap-sourced texture and sampler handles, selecting
//  channel %int_0/1/2/3 via OpImageGather %v4float.

// CHECK-DAG:     %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:      %[[Tex2DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:    %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG: %[[SampledImgType:[a-zA-Z0-9_]+]] = OpTypeSampledImage %[[Tex2DType]]
// CHECK-DAG:       %[[RA_Tex2D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DType]]{{$}}
// CHECK-DAG:     %[[RA_Sampler:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]{{$}}

// CHECK-DAG:   %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant
// CHECK-DAG:    %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

float4 main(float2 uv : TEXCOORD0) : SV_Target {
  Texture2D<float4> tex = ResourceDescriptorHeap[0];
  SamplerState samp = SamplerDescriptorHeap[0];

  // Tex and sampler are each loaded once from the heap; each Gather* creates a
  // fresh OpSampledImage but reuses the same loaded image and sampler handles.
  // CHECK:         %[[TexChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex2D]] %[[ResourceHeap]] %uint_0
  // CHECK:             %[[TexH:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2DType]] %[[TexChain]]
  // CHECK:        %[[SampChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Sampler]] %[[SamplerHeap]] %uint_0
  // CHECK:            %[[SampH:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SampChain]]

  // CHECK:              %[[SI0:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageGather %v4float %[[SI0]] {{.*}} %int_0
  float4 r = tex.GatherRed(samp, uv);
  // CHECK:              %[[SI1:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageGather %v4float %[[SI1]] {{.*}} %int_1
  float4 g = tex.GatherGreen(samp, uv);
  // CHECK:              %[[SI2:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageGather %v4float %[[SI2]] {{.*}} %int_2
  float4 b = tex.GatherBlue(samp, uv);
  // CHECK:              %[[SI3:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageGather %v4float %[[SI3]] {{.*}} %int_3
  float4 a = tex.GatherAlpha(samp, uv);

  return r + g + b + a;
}
