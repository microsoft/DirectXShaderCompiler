// RUN: %dxc -T ps_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: NonUniformResourceIndex on descriptor-heap accesses does NOT
//  propagate NonUniform to the access-chain result or the loaded value.
//  SPV_EXT_descriptor_heap deprecates the NonUniform decoration; drivers handle
//  divergent indices natively and the decoration must be omitted.

// CHECK-DAG: %[[UntypedPtrType:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:      %[[Tex2DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:    %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:   %[[RA_Tex2DType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DType]]
// CHECK-DAG: %[[RA_SamplerType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]

// CHECK-NOT:                                     OpDecorate %{{.*}} NonUniform

// CHECK:       %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtrType]] UniformConstant
// CHECK:        %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtrType]] UniformConstant

float4 main(uint idx : A) : SV_Target {
  Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(idx)];
  SamplerState samp = SamplerDescriptorHeap[NonUniformResourceIndex(idx + 1)];

  // CHECK:         %[[TexChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_Tex2DType]] %[[ResourceHeap]] %{{.*}}
  // CHECK:        %[[TexLoaded:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2DType]] %[[TexChain]]
  // CHECK:        %[[SampChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_SamplerType]] %[[SamplerHeap]] %{{.*}}
  // CHECK:       %[[SampLoaded:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SampChain]]
  // CHECK:         %[[Combined:[a-zA-Z0-9_]+]] = OpSampledImage %{{.*}} %[[TexLoaded]] %[[SampLoaded]]
  // CHECK:                                       OpImageSampleExplicitLod %v4float %[[Combined]]
  return tex.SampleLevel(samp, float2(0.0, 0.0), 0.0);
}
