// RUN: %dxc -T cs_6_2 -E main -enable-16bit-types

ByteAddressBuffer buf;

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId)
{
// ********* 16-bit vector ********************
// CHECK:   [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:  [[byteOff0:%\d+]] = OpUMod %uint [[addr0]] %uint_4
// CHECK:   [[bitOff0:%\d+]] = OpShiftLeftLogical %uint [[byteOff0]] %uint_3
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:     [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[val0_uint:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff0]]
// CHECK:      [[val0:%\d+]] = OpUConvert %ushort [[val0_uint]]
// CHECK:     [[addr1:%\d+]] = OpIAdd %uint [[addr0]] %uint_2

// CHECK:   [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr1]] %uint_2
// CHECK:  [[byteOff1:%\d+]] = OpUMod %uint [[addr1]] %uint_4
// CHECK:   [[bitOff1:%\d+]] = OpShiftLeftLogical %uint [[byteOff1]] %uint_3
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:     [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[val1_uint:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff1]]
// CHECK:      [[val1:%\d+]] = OpUConvert %ushort [[val1_uint]]
// CHECK:     [[addr2:%\d+]] = OpIAdd %uint [[addr1]] %uint_2

// CHECK:   [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr2:%\d+]] %uint_2
// CHECK:  [[byteOff2:%\d+]] = OpUMod %uint [[addr2]] %uint_4
// CHECK:   [[bitOff2:%\d+]] = OpShiftLeftLogical %uint [[byteOff2]] %uint_3
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:     [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[val2_uint:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOff2]]
// CHECK:      [[val2:%\d+]] = OpUConvert %ushort [[val2_uint]]

// CHECK:      [[uVec:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val1]] [[val2]]
// CHECK:                      OpStore %u16 [[uVec]]
  uint16_t3 u16 = buf.Load<uint16_t3>(tid.x);


// ********* 32-bit vector ********************

// CHECK: [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:   [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val0:%\d+]] = OpBitcast %int [[word0]]
// CHECK: [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:   [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val1:%\d+]] = OpBitcast %int [[word1]]
// CHECK:    [[iVec:%\d+]] = OpCompositeConstruct %v2int [[val0]] [[val1]]
// CHECK:                    OpStore %i [[iVec]]
  int2 i = buf.Load<int2>(tid.x);

// CHECK: [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:   [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val0:%\d+]] = OpINotEqual %bool [[word0]] %uint_0
// CHECK: [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:   [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val1:%\d+]] = OpINotEqual %bool [[word1]] %uint_0
// CHECK:    [[bVec:%\d+]] = OpCompositeConstruct %v2bool [[val0]] [[val1]]
// CHECK:                    OpStore %b [[bVec]]
  bool2 b = buf.Load<bool2>(tid.x);

// ********* 64-bit vector ********************

// CHECK:              [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:                  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:                [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:              [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:                  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:                [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[word0_ulong:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:          [[word1_ulong:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK:  [[shifted_word1_ulong:%\d+]] = OpShiftLeftLogical %ulong [[word1_ulong]] %uint_32
// CHECK:           [[val0_ulong:%\d+]] = OpBitwiseOr %ulong [[word0_ulong]] [[shifted_word1_ulong]]
// CHECK:                 [[val0:%\d+]] = OpBitcast %double [[val0_ulong]]
// CHECK:              [[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:                  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:                [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:              [[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:                  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:                [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[word0_ulong:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:          [[word1_ulong:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK:  [[shifted_word1_ulong:%\d+]] = OpShiftLeftLogical %ulong [[word1_ulong]] %uint_32
// CHECK:           [[val1_ulong:%\d+]] = OpBitwiseOr %ulong [[word0_ulong]] [[shifted_word1_ulong]]
// CHECK:                 [[val1:%\d+]] = OpBitcast %double [[val1_ulong]]
// CHECK:                 [[fVec:%\d+]] = OpCompositeConstruct %v2double [[val0]] [[val1]]
// CHECK:                                 OpStore %f64 [[fVec]]
  float64_t2 f64 = buf.Load<float64_t2>(tid.x);

// ********* array of vectors ********************

// CHECK:       [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr0]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[val0_uint:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff]]
// CHECK:          [[val0:%\d+]] = OpUConvert %ushort [[val0_uint]]
// CHECK:         [[addr1:%\d+]] = OpIAdd %uint [[addr0]] %uint_2

// CHECK:       [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr1]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr1]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word0_shifted:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff]]
// CHECK:          [[val1:%\d+]] = OpUConvert %ushort [[word0_shifted]]
// CHECK:         [[addr2:%\d+]] = OpIAdd %uint [[addr1]] %uint_2

// CHECK:       [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr2]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr2]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word1_shifted:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOff]]
// CHECK:          [[val2:%\d+]] = OpUConvert %ushort [[word1_shifted]]
// CHECK:         [[addr3:%\d+]] = OpIAdd %uint [[addr2]] %uint_2

// CHECK:          [[vec0:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val1]] [[val2]]

// CHECK:       [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr3]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr3]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word1_shifted:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOff]]
// CHECK:          [[val3:%\d+]] = OpUConvert %ushort [[word1_shifted:%\d+]]
// CHECK:         [[addr4:%\d+]] = OpIAdd %uint [[addr3]] %uint_2

// CHECK:       [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr4]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr4]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word2_shifted:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOff]]
// CHECK:          [[val4:%\d+]] = OpUConvert %ushort [[word2_shifted]]
// CHECK:         [[addr5:%\d+]] = OpIAdd %uint [[addr4]] %uint_2

// CHECK:       [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr5]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr5]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word2:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOff]]
// CHECK:          [[val5:%\d+]] = OpUConvert %ushort [[shifted_word2]]
// CHECK:         [[addr6:%\d+]] = OpIAdd %uint [[addr5]] %uint_2

// CHECK:          [[vec1:%\d+]] = OpCompositeConstruct %v3ushort [[val3]] [[val4]] [[val5]]

// CHECK:       [[index_3:%\d+]] = OpShiftRightLogical %uint [[addr6]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr6]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:         [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word3_shifted:%\d+]] = OpShiftRightLogical %uint [[word3]] [[bitOff]]
// CHECK:          [[val6:%\d+]] = OpUConvert %ushort [[word3_shifted]]
// CHECK:         [[addr7:%\d+]] = OpIAdd %uint [[addr6]] %uint_2

// CHECK:       [[index_3:%\d+]] = OpShiftRightLogical %uint [[addr7]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr7]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:         [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word3:%\d+]] = OpShiftRightLogical %uint [[word3]] [[bitOff]]
// CHECK:          [[val7:%\d+]] = OpUConvert %ushort [[shifted_word3]]
// CHECK:         [[addr8:%\d+]] = OpIAdd %uint [[addr7]] %uint_2

// CHECK:       [[index_4:%\d+]] = OpShiftRightLogical %uint [[addr8]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr8]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:         [[word4:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word4_shifted:%\d+]] = OpShiftRightLogical %uint [[word4]] [[bitOff]]
// CHECK:          [[val8:%\d+]] = OpUConvert %ushort [[word4_shifted]]

// CHECK:          [[vec2:%\d+]] = OpCompositeConstruct %v3ushort [[val6]] [[val7]] [[val8]]
// CHECK:        [[vecArr:%\d+]] = OpCompositeConstruct %_arr_v3ushort_uint_3 [[vec0]] [[vec1]] [[vec2]]
// CHECK:                          OpStore %uVec [[vecArr]]
  uint16_t3 uVec[3] = buf.Load<uint16_t3[3]>(tid.x);
}

