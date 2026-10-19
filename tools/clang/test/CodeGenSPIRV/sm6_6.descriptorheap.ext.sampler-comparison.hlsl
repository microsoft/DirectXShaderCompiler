// RUN: %dxc -T ps_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: a heap-sourced SamplerComparisonState combined with a heap-sourced
//  Texture2D drives OpImageSampleDrefExplicitLod (SampleCmpLevelZero), 
//  with the comparison sampler loaded from the sampler heap and joined 
//  via OpSampledImage.

// CHECK-DAG:  %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:     %[[TexType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG: %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:      %[[RA_Tex:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexType]]{{$}}
// CHECK-DAG:  %[[RA_Sampler:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]{{$}}

// CHECK:    %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant
// CHECK:     %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

float4 main(float2 uv : TEXCOORD0) : SV_Target {
  Texture2D<float> depthTex = ResourceDescriptorHeap[0];
  // CHECK:       %[[TexDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex]] %[[ResourceHeap]] %uint_0
  // CHECK:     %[[TexHandle:[a-zA-Z0-9_]+]] = OpLoad %[[TexType]] %[[TexDesc]]

  SamplerComparisonState shadowSamp = SamplerDescriptorHeap[0];
  // CHECK:      %[[SampDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Sampler]] %[[SamplerHeap]] %uint_0
  // CHECK:    %[[SampHandle:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SampDesc]]

  // CHECK:      %[[Combined:[a-zA-Z0-9_]+]] = OpSampledImage %{{[a-zA-Z0-9_]+}} %[[TexHandle]] %[[SampHandle]]
  // CHECK:        %[[Shadow:[a-zA-Z0-9_]+]] = OpImageSampleDrefExplicitLod %float %[[Combined]]
  float shadow = depthTex.SampleCmpLevelZero(shadowSamp, uv, 0.5);
  return float4(shadow, shadow, shadow, 1.0);
}
