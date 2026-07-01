// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: each HLSL resource element type lowers to the 
//  correct OpTypeImage format and sampled-vs-storage mode 
//  when accessed through the descriptor heap.
//
// Texture2D<uint>     -> OpTypeImage %uint  2D ... Unknown -> sampled
// RWTexture2D<float2> -> OpTypeImage %float 2D ... Rg32f   -> storage
// RWTexture2D<uint2>  -> OpTypeImage %uint  2D ... Rg32ui  -> storage
// RWTexture2D<int>    -> OpTypeImage %int   2D ... R32i    -> storage

// CHECK-DAG:  %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant

// CHECK-DAG: %[[TexUintType:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 1 Unknown
// CHECK-DAG:  %[[RA_TexUint:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexUintType]]{{$}}

// CHECK-DAG: %[[RWTexF2Type:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 2 Rg32f
// CHECK-DAG:  %[[RA_RWTexF2:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexF2Type]]{{$}}

// CHECK-DAG: %[[RWTexU2Type:[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 2 0 0 2 Rg32ui
// CHECK-DAG:  %[[RA_RWTexU2:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexU2Type]]{{$}}

// CHECK-DAG:  %[[RWTexIType:[a-zA-Z0-9_]+]] = OpTypeImage %int 2D 2 0 0 2 R32i
// CHECK-DAG:   %[[RA_RWTexI:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWTexIType]]{{$}}

// Default heap stride: OpConstantSizeOfEXT + ArrayStrideIdEXT per element type.
// CHECK-DAG: %[[TexUintSize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[TexUintType]]
// CHECK-DAG: %[[RWTexF2Size:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[RWTexF2Type]]
// CHECK-DAG: %[[RWTexU2Size:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[RWTexU2Type]]
// CHECK-DAG:  %[[RWTexISize:[a-zA-Z0-9_]+]] = OpConstantSizeOfEXT %uint %[[RWTexIType]]
// CHECK-DAG:                                  OpDecorateId %[[RA_TexUint]] ArrayStrideIdEXT %[[TexUintSize]]
// CHECK-DAG:                                  OpDecorateId %[[RA_RWTexF2]] ArrayStrideIdEXT %[[RWTexF2Size]]
// CHECK-DAG:                                  OpDecorateId %[[RA_RWTexU2]] ArrayStrideIdEXT %[[RWTexU2Size]]
// CHECK-DAG:                                  OpDecorateId %[[RA_RWTexI]] ArrayStrideIdEXT %[[RWTexISize]]

// CHECK:    %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

RWByteAddressBuffer output : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture2D<uint> texUint = ResourceDescriptorHeap[0];
  // CHECK:  %[[TexUintChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_TexUint]] %[[ResourceHeap]] %uint_0
  // CHECK:      %[[TexUintH:[a-zA-Z0-9_]+]] = OpLoad %[[TexUintType]] %[[TexUintChain]]

  RWTexture2D<float2> rwTexF2 = ResourceDescriptorHeap[1];
  // CHECK:  %[[RWTexF2Chain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RWTexF2]] %[[ResourceHeap]] %uint_1
  // CHECK:      %[[RWTexF2H:[a-zA-Z0-9_]+]] = OpLoad %[[RWTexF2Type]] %[[RWTexF2Chain]]

  RWTexture2D<uint2> rwTexU2 = ResourceDescriptorHeap[2];
  // CHECK:  %[[RWTexU2Chain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RWTexU2]] %[[ResourceHeap]] %uint_2
  // CHECK:      %[[RWTexU2H:[a-zA-Z0-9_]+]] = OpLoad %[[RWTexU2Type]] %[[RWTexU2Chain]]

  RWTexture2D<int> rwTexI = ResourceDescriptorHeap[3];
  // CHECK:   %[[RWTexIChain:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[RA_RWTexI]] %[[ResourceHeap]] %uint_3
  // CHECK:       %[[RWTexIH:[a-zA-Z0-9_]+]] = OpLoad %[[RWTexIType]] %[[RWTexIChain]]

  // CHECK:                                    OpImageFetch %v4uint %[[TexUintH]]
  uint val = texUint.Load(int3(tid.xy, 0)).x;

  // CHECK:                                    OpImageWrite %[[RWTexF2H]]
  rwTexF2[tid.xy] = float2(val, val);
  // CHECK:                                    OpImageWrite %[[RWTexU2H]]
  rwTexU2[tid.xy] = uint2(val, val);
  // CHECK:                                    OpImageWrite %[[RWTexIH]]
  rwTexI[tid.xy] = int(val);

  output.Store(0, val);
}
