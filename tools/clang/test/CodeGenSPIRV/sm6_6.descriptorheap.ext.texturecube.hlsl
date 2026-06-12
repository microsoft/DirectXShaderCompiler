// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: TextureCube and TextureCubeArray cube image types, 
//  each loaded from the resource heap and combined with a heap 
//  sampler to drive OpImageSampleExplicitLod.

// CHECK-DAG:  %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:    %[[CubeType:[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 2 0 0 1 Unknown
// CHECK-DAG: %[[CubeArrType:[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 2 1 0 1 Unknown
// CHECK-DAG: %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler

// CHECK-DAG:     %[[RA_Cube:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[CubeType]]{{$}}
// CHECK-DAG:  %[[RA_CubeArr:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[CubeArrType]]{{$}}
// CHECK-DAG:  %[[RA_Sampler:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]{{$}}

// CHECK:    %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant
// CHECK:     %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

RWByteAddressBuffer output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  TextureCube<float4> cube = ResourceDescriptorHeap[0];
  // CHECK:      %[[CubeDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Cube]] %[[ResourceHeap]] %uint_0
  // CHECK:    %[[CubeHandle:[a-zA-Z0-9_]+]] = OpLoad %[[CubeType]] %[[CubeDesc]]

  TextureCubeArray<float4> cubeArr = ResourceDescriptorHeap[1];
  // CHECK:   %[[CubeArrDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_CubeArr]] %[[ResourceHeap]] %uint_1
  // CHECK: %[[CubeArrHandle:[a-zA-Z0-9_]+]] = OpLoad %[[CubeArrType]] %[[CubeArrDesc]]

  SamplerState samp = SamplerDescriptorHeap[0];
  // CHECK:      %[[SampDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Sampler]] %[[SamplerHeap]] %uint_0
  // CHECK:    %[[SampHandle:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SampDesc]]

  float3 dir = normalize(float3(tid));

  // CHECK:        %[[CubeSI:[a-zA-Z0-9_]+]] = OpSampledImage %{{.*}} %[[CubeHandle]] %[[SampHandle]]
  // CHECK:                                    OpImageSampleExplicitLod %v4float %[[CubeSI]]
  float4 v = cube.SampleLevel(samp, dir, 0);

  // CHECK:     %[[CubeArrSI:[a-zA-Z0-9_]+]] = OpSampledImage %{{.*}} %[[CubeArrHandle]] %[[SampHandle]]
  // CHECK:                                    OpImageSampleExplicitLod %v4float %[[CubeArrSI]]
  v += cubeArr.SampleLevel(samp, float4(dir, 0), 0);

  output.Store(0, asuint(v.x));
}
