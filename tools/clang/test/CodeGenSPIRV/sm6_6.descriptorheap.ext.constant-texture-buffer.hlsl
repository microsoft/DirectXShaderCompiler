// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: ConstantBuffer and TextureBuffer from the heap map to different element types, 
//  both carrying Block and supporting member access.
//
// ConstantBuffer -> OpTypeBufferEXT Uniform       -> Block
// TextureBuffer  -> OpTypeBufferEXT StorageBuffer -> Block + NonWritable members

// CHECK-DAG:                                             OpDecorate %type_ConstantBuffer_Data Block
// CHECK-DAG:                                             OpDecorate %type_TextureBuffer_Data Block
// CHECK-DAG:                                             OpMemberDecorate %type_TextureBuffer_Data 0 NonWritable
// CHECK-DAG:                                             OpMemberDecorate %type_TextureBuffer_Data 1 NonWritable

// CHECK-DAG: %[[UntypedUniformConstant:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:                 %[[CBDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT Uniform
// CHECK-DAG:            %[[CBDescArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[CBDesc]]
// CHECK-DAG:                  %[[CBPtr:[a-zA-Z0-9_]+]] = OpTypePointer Uniform %type_ConstantBuffer_Data
// CHECK-DAG:                 %[[TBDesc:[a-zA-Z0-9_]+]] = OpTypeBufferEXT StorageBuffer
// CHECK-DAG:            %[[TBDescArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[TBDesc]]
// CHECK-DAG:                  %[[TBPtr:[a-zA-Z0-9_]+]] = OpTypePointer StorageBuffer %type_TextureBuffer_Data

// CHECK:               %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedUniformConstant]] UniformConstant

struct Data {
  uint a;
  uint b[2];
};

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  ConstantBuffer<Data> cb = ResourceDescriptorHeap[0];
  TextureBuffer<Data> tb = ResourceDescriptorHeap[1];

  uint index = tid.x & 1;

  // CHECK:                %[[CBDescPtr:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[CBDescArray]] %[[ResourceHeap]] %uint_0
  // CHECK:                                               OpBufferPointerEXT %[[CBPtr]] %[[CBDescPtr]]
  // CHECK:                %[[TBDescPtr:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedUniformConstant]] %[[TBDescArray]] %[[ResourceHeap]] %uint_1
  // CHECK:                                               OpBufferPointerEXT %[[TBPtr]] %[[TBDescPtr]]
  // CHECK:                  %[[CBDataA:[a-zA-Z0-9_]+]] = OpBufferPointerEXT %[[CBPtr]] %[[CBDescPtr]]
  // CHECK:                                               OpAccessChain %{{[a-zA-Z0-9_]+}} %[[CBDataA]] %int_0
  // CHECK:                  %[[CBDataB:[a-zA-Z0-9_]+]] = OpBufferPointerEXT %[[CBPtr]] %[[CBDescPtr]]
  // CHECK:                                               OpAccessChain %{{[a-zA-Z0-9_]+}} %[[CBDataB]] %int_1 %{{[a-zA-Z0-9_]+}}
  // CHECK:                  %[[TBDataA:[a-zA-Z0-9_]+]] = OpBufferPointerEXT %[[TBPtr]] %[[TBDescPtr]]
  // CHECK:                                               OpAccessChain %{{[a-zA-Z0-9_]+}} %[[TBDataA]] %int_0
  // CHECK:                  %[[TBDataB:[a-zA-Z0-9_]+]] = OpBufferPointerEXT %[[TBPtr]] %[[TBDescPtr]]
  // CHECK:                                               OpAccessChain %{{[a-zA-Z0-9_]+}} %[[TBDataB]] %int_1 %int_1
  outputBytes.Store(0, cb.a + cb.b[index] + tb.a + tb.b[1]);
}
