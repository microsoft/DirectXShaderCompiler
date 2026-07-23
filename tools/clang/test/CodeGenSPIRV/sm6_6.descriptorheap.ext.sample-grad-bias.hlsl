// RUN: %dxc -T ps_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: SampleGrad lowers to OpImageSampleExplicitLod with the Grad image
//  operand and SampleBias lowers to OpImageSampleImplicitLod with the Bias 
//  image operand, where the texture and sampler handles are loaded once from
//  the descriptor heaps and reused across both samples (each sample a fresh
//  OpSampledImage).

// Capture anchors only (heap-source decorations owned by mixed-bound).
// CHECK-DAG:                                     OpDecorate %[[ResourceHeap:[a-zA-Z0-9_]+]] BuiltIn ResourceHeapEXT
// CHECK-DAG:                                     OpDecorate %[[SamplerHeap:[a-zA-Z0-9_]+]] BuiltIn SamplerHeapEXT

// CHECK-DAG:     %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:      %[[Tex2DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:    %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG: %[[SampledImgType:[a-zA-Z0-9_]+]] = OpTypeSampledImage %[[Tex2DType]]
// CHECK-DAG:       %[[RA_Tex2D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DType]]{{$}}
// CHECK-DAG:     %[[RA_Sampler:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]{{$}}

float4 main(float2 uv : TEXCOORD0) : SV_Target {
  Texture2D<float4> tex = ResourceDescriptorHeap[0];
  SamplerState samp = SamplerDescriptorHeap[0];

  // Tex and sampler each loaded once; each sample op creates its own OpSampledImage
  // but both reuse the same loaded handles.
  // CHECK:         %[[TexChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex2D]] %[[ResourceHeap]] %uint_0
  // CHECK:             %[[TexH:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2DType]] %[[TexChain]]
  // CHECK:        %[[SampChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Sampler]] %[[SamplerHeap]] %uint_0
  // CHECK:            %[[SampH:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SampChain]]

  // CHECK:              %[[SI0:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageSampleExplicitLod %v4float %[[SI0]] {{.*}} Grad
  float4 a = tex.SampleGrad(samp, uv, ddx(uv), ddy(uv));

  // CHECK:              %[[SI1:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImgType]] %[[TexH]] %[[SampH]]
  // CHECK:                                       OpImageSampleImplicitLod %v4float %[[SI1]] {{.*}} Bias
  float4 b = tex.SampleBias(samp, uv, -1.0);

  return a + b;
}
