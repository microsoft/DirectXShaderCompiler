// RUN: %dxc -T ps_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: Texture.Load with a constant offset emits the ConstOffset 
//  image operand on OpImageFetch, contrasted against the no-offset fetch baseline.

// CHECK-DAG: %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:  %[[Tex2DType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:   %[[RA_Tex2D:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2DType]]{{$}}

float4 main(float4 pos : SV_Position) : SV_Target {
  Texture2D<float4> tex = ResourceDescriptorHeap[0];

  // CHECK:                                   OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_Tex2D]] %[[ResourceHeap:[a-zA-Z0-9_]+]] %uint_0
  // CHECK:       %[[Handle:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2DType]]

  int3 coord = int3(pos.xy, 0);

  // Load without offset.
  // CHECK:                                   OpImageFetch %v4float %[[Handle]]
  float4 a = tex.Load(coord);

  // Load with constant offset — the ConstOffset image operand must appear.
  // CHECK:                                   OpImageFetch %v4float %[[Handle]] {{.*}}ConstOffset
  float4 b = tex.Load(coord, int2(1, -1));

  return a + b;
}
