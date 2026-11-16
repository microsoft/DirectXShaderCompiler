// RUN: %dxc -T cs_6_6 -E main -Od -fcgl -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: a heap-loaded resource passed by value to a user function lowers to
// pass-by-Function-pointer.
//
//   heap handle    -> OpStore into a Function-class var -> per-call OpLoad/OpStore/OpFunctionCall
//   parameter slot -> OpFunctionParameter %[[*PtrType]] -> Function storage class

// CHECK-DAG:                                     OpName %[[ReadTex:[a-zA-Z0-9_]+]] "ReadTex"
// CHECK-DAG:                                     OpName %[[ReadBuf:[a-zA-Z0-9_]+]] "ReadBuf"
// CHECK-DAG:                                     OpName %[[WriteBuf:[a-zA-Z0-9_]+]] "WriteBuf"

// CHECK-DAG: %[[UntypedPtrType:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:        %[[TexType:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:        %[[BufType:[a-zA-Z0-9_]+]] = OpTypeImage %float Buffer 2 0 0 1 Rgba32f
// CHECK-DAG:      %[[RWBufType:[a-zA-Z0-9_]+]] = OpTypeImage %float Buffer 2 0 0 2 Rgba32f
// CHECK-DAG:    %[[SamplerType:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:     %[[TexPtrType:[a-zA-Z0-9_]+]] = OpTypePointer Function %[[TexType]]
// CHECK-DAG:     %[[BufPtrType:[a-zA-Z0-9_]+]] = OpTypePointer Function %[[BufType]]
// CHECK-DAG:   %[[RWBufPtrType:[a-zA-Z0-9_]+]] = OpTypePointer Function %[[RWBufType]]
// CHECK-DAG: %[[SamplerPtrType:[a-zA-Z0-9_]+]] = OpTypePointer Function %[[SamplerType]]
// CHECK-DAG:     %[[RA_TexType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TexType]]
// CHECK-DAG:     %[[RA_BufType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[BufType]]
// CHECK-DAG:   %[[RA_RWBufType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[RWBufType]]
// CHECK-DAG: %[[RA_SamplerType:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[SamplerType]]

float4 ReadTex(Texture2D<float4> tex, SamplerState samp);
float4 ReadBuf(Buffer<float4> buf, uint index);
void WriteBuf(RWBuffer<float4> buf, uint index, float4 value);

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  Texture2D<float4> tex = ResourceDescriptorHeap[0];
  Buffer<float4> buf = ResourceDescriptorHeap[1];
  RWBuffer<float4> outBuf = ResourceDescriptorHeap[2];
  SamplerState samp = SamplerDescriptorHeap[0];

  // CHECK:     %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtrType]] UniformConstant
  // CHECK:      %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtrType]] UniformConstant

  // CHECK:          %[[TexDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_TexType]] %[[ResourceHeap]] %uint_0
  // CHECK:        %[[TexHandle:[a-zA-Z0-9_]+]] = OpLoad %[[TexType]] %[[TexDesc]]
  // CHECK:                                       OpStore %[[TexVar:[a-zA-Z0-9_]+]] %[[TexHandle]]
  // CHECK:          %[[BufDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_BufType]] %[[ResourceHeap]] %uint_1
  // CHECK:        %[[BufHandle:[a-zA-Z0-9_]+]] = OpLoad %[[BufType]] %[[BufDesc]]
  // CHECK:                                       OpStore %[[BufVar:[a-zA-Z0-9_]+]] %[[BufHandle]]
  // CHECK:        %[[RWBufDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_RWBufType]] %[[ResourceHeap]] %uint_2
  // CHECK:      %[[RWBufHandle:[a-zA-Z0-9_]+]] = OpLoad %[[RWBufType]] %[[RWBufDesc]]
  // CHECK:                                       OpStore %[[RWBufVar:[a-zA-Z0-9_]+]] %[[RWBufHandle]]
  // CHECK:      %[[SamplerDesc:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtrType]] %[[RA_SamplerType]] %[[SamplerHeap]] %uint_0
  // CHECK:    %[[SamplerHandle:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SamplerDesc]]
  // CHECK:                                       OpStore %[[SamplerVar:[a-zA-Z0-9_]+]] %[[SamplerHandle]]

  // CHECK:           %[[TexArg:[a-zA-Z0-9_]+]] = OpLoad %[[TexType]] %[[TexVar]]
  // CHECK:                                       OpStore %[[TexParam:[a-zA-Z0-9_]+]] %[[TexArg]]
  // CHECK:       %[[SamplerArg:[a-zA-Z0-9_]+]] = OpLoad %[[SamplerType]] %[[SamplerVar]]
  // CHECK:                                       OpStore %[[SamplerParam:[a-zA-Z0-9_]+]] %[[SamplerArg]]
  // CHECK:         %[[TexValue:[a-zA-Z0-9_]+]] = OpFunctionCall %v4float %[[ReadTex]] %[[TexParam]] %[[SamplerParam]]
  float4 value = ReadTex(tex, samp);

  // CHECK:           %[[BufArg:[a-zA-Z0-9_]+]] = OpLoad %[[BufType]] %[[BufVar]]
  // CHECK:                                       OpStore %[[BufParam:[a-zA-Z0-9_]+]] %[[BufArg]]
  // CHECK:         %[[BufValue:[a-zA-Z0-9_]+]] = OpFunctionCall %v4float %[[ReadBuf]] %[[BufParam]]
  value += ReadBuf(buf, tid.x);

  // CHECK:         %[[RWBufArg:[a-zA-Z0-9_]+]] = OpLoad %[[RWBufType]] %[[RWBufVar]]
  // CHECK:                                       OpStore %[[RWBufParam:[a-zA-Z0-9_]+]] %[[RWBufArg]]
  // CHECK:                                       OpFunctionCall %void %[[WriteBuf]] %[[RWBufParam]]
  WriteBuf(outBuf, tid.x, value);

  outputBytes.Store(tid.x * 4, asuint(value.x));
}

// Callees emit after the entry point; each receives its resource by Function pointer.
float4 ReadTex(Texture2D<float4> tex, SamplerState samp) {
  // CHECK:                        %[[ReadTex]] = OpFunction %v4float
  // CHECK:                                       OpFunctionParameter %[[TexPtrType]]
  // CHECK:                                       OpFunctionParameter %[[SamplerPtrType]]
  // CHECK:                                       OpSampledImage
  return tex.SampleLevel(samp, float2(0.0, 0.0), 0.0);
}

float4 ReadBuf(Buffer<float4> buf, uint index) {
  // CHECK:                        %[[ReadBuf]] = OpFunction %v4float
  // CHECK:                                       OpFunctionParameter %[[BufPtrType]]
  // CHECK:                                       OpImageFetch
  return buf.Load(index);
}

void WriteBuf(RWBuffer<float4> buf, uint index, float4 value) {
  // CHECK:                       %[[WriteBuf]] = OpFunction %void
  // CHECK:                                       OpFunctionParameter %[[RWBufPtrType]]
  // CHECK:                                       OpImageWrite
  buf[index] = value;
}
