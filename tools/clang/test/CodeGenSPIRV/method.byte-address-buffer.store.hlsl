// Run: %dxc -T cs_6_0 -E main

RWByteAddressBuffer outBuffer;

[numthreads(1, 1, 1)]
void main() {
  uint addr = 0;
  uint words1 = 1;
  uint2 words2 = uint2(1, 2);
  uint3 words3 = uint3(1, 2, 3);
  uint4 words4 = uint4(1, 2, 3, 4);

// CHECK:      [[byteAddr1:%\d+]] = OpLoad %uint %addr
// CHECK-NEXT: [[baseAddr1:%\d+]] = OpShiftRightLogical %uint [[byteAddr1]] %uint_2
// CHECK-NEXT: [[words1:%\d+]] = OpLoad %uint %words1
// CHECK-NEXT: [[out1_outBufPtr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr1]]
// CHECK-NEXT: OpStore [[out1_outBufPtr0]] [[words1]]
  outBuffer.Store(addr, words1);


// CHECK:      [[byteAddr2:%\d+]] = OpLoad %uint %addr
// CHECK-NEXT: [[baseAddr2:%\d+]] = OpShiftRightLogical %uint [[byteAddr2]] %uint_2
// CHECK-NEXT: [[words2:%\d+]] = OpLoad %v2uint %words2
// CHECK-NEXT: [[words2_0:%\d+]] = OpCompositeExtract %uint [[words2]] 0
// CHECK-NEXT: [[out2_outBufPtr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr2]]
// CHECK-NEXT: OpStore [[out2_outBufPtr0]] [[words2_0]]
// CHECK-NEXT: [[words2_1:%\d+]] = OpCompositeExtract %uint [[words2]] 1
// CHECK-NEXT: [[baseAddr2_plus1:%\d+]] = OpIAdd %uint [[baseAddr2]] %uint_1
// CHECK-NEXT: [[out2_outBufPtr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr2_plus1]]
// CHECK-NEXT: OpStore [[out2_outBufPtr1]] [[words2_1]]
  outBuffer.Store2(addr, words2);


// CHECK:      [[byteAddr3:%\d+]] = OpLoad %uint %addr
// CHECK-NEXT: [[baseAddr3:%\d+]] = OpShiftRightLogical %uint [[byteAddr3]] %uint_2
// CHECK-NEXT: [[words3:%\d+]] = OpLoad %v3uint %words3
// CHECK-NEXT: [[word3_0:%\d+]] = OpCompositeExtract %uint [[words3]] 0
// CHECK-NEXT: [[out3_outBufPtr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr3]]
// CHECK-NEXT: OpStore [[out3_outBufPtr0]] [[word3_0]]
// CHECK-NEXT: [[words3_1:%\d+]] = OpCompositeExtract %uint [[words3]] 1
// CHECK-NEXT: [[baseAddr3_plus1:%\d+]] = OpIAdd %uint [[baseAddr3]] %uint_1
// CHECK-NEXT: [[out3_outBufPtr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr3_plus1]]
// CHECK-NEXT: OpStore [[out3_outBufPtr1]] [[words3_1]]
// CHECK-NEXT: [[word3_2:%\d+]] = OpCompositeExtract %uint [[words3]] 2
// CHECK-NEXT: [[baseAddr3_plus2:%\d+]] = OpIAdd %uint [[baseAddr3]] %uint_2
// CHECK-NEXT: [[out3_outBufPtr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr3_plus2]]
// CHECK-NEXT: OpStore [[out3_outBufPtr2]] [[word3_2]]
  outBuffer.Store3(addr, words3);


// CHECK:      [[byteAddr:%\d+]] = OpLoad %uint %addr
// CHECK-NEXT: [[baseAddr:%\d+]] = OpShiftRightLogical %uint [[byteAddr]] %uint_2
// CHECK-NEXT: [[words4:%\d+]] = OpLoad %v4uint %words4
// CHECK-NEXT: [[word0:%\d+]] = OpCompositeExtract %uint [[words4]] 0
// CHECK-NEXT: [[outBufPtr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr]]
// CHECK-NEXT: OpStore [[outBufPtr0]] [[word0]]
// CHECK-NEXT: [[word1:%\d+]] = OpCompositeExtract %uint [[words4]] 1
// CHECK-NEXT: [[baseAddr_plus1:%\d+]] = OpIAdd %uint [[baseAddr]] %uint_1
// CHECK-NEXT: [[outBufPtr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr_plus1]]
// CHECK-NEXT: OpStore [[outBufPtr1]] [[word1]]
// CHECK-NEXT: [[word2:%\d+]] = OpCompositeExtract %uint [[words4]] 2
// CHECK-NEXT: [[baseAddr_plus2:%\d+]] = OpIAdd %uint [[baseAddr]] %uint_2
// CHECK-NEXT: [[outBufPtr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr_plus2]]
// CHECK-NEXT: OpStore [[outBufPtr2]] [[word2]]
// CHECK-NEXT: [[word3:%\d+]] = OpCompositeExtract %uint [[words4]] 3
// CHECK-NEXT: [[baseAddr_plus3:%\d+]] = OpIAdd %uint [[baseAddr]] %uint_3
// CHECK-NEXT: [[outBufPtr3:%\d+]] = OpAccessChain %_ptr_Uniform_uint %outBuffer %uint_0 [[baseAddr_plus3]]
// CHECK-NEXT: OpStore [[outBufPtr3]] [[word3]]
  outBuffer.Store4(addr, words4);
}
