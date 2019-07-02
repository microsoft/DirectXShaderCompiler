// Run: %dxc -T cs_6_2 -E main -enable-16bit-types

ByteAddressBuffer buf;

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId)
{
// ********* 16-bit vector ********************
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:     [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:      [[val0:%\d+]] = OpUConvert %ushort [[word0]]
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:     [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[val1_uint:%\d+]] = OpShiftRightLogical %uint [[word0]] %uint_16
// CHECK:      [[val1:%\d+]] = OpUConvert %ushort [[val1_uint]]
// CHECK:   [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:     [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:      [[val2:%\d+]] = OpUConvert %ushort [[word1]]
// CHECK:      [[uVec:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val1]] [[val2]]
// CHECK:                      OpStore %u16 [[uVec]]
  uint16_t3 u16 = buf.Load<uint16_t3>(tid.x);


// ********* 32-bit vector ********************

// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:   [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val0:%\d+]] = OpBitcast %int [[word0]]
// CHECK: [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:   [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:    [[val1:%\d+]] = OpBitcast %int [[word1]]
// CHECK:    [[iVec:%\d+]] = OpCompositeConstruct %v2int [[val0]] [[val1]]
// CHECK:                    OpStore %i [[iVec]]
  int2 i = buf.Load<int2>(tid.x);

// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
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

// CHECK:                  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
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

// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val0:%\d+]] = OpUConvert %ushort [[word0]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 %117
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word0_shifted:%\d+]] = OpShiftRightLogical %uint [[word0]] %uint_16
// CHECK:          [[val1:%\d+]] = OpUConvert %ushort [[word0_shifted]]
// CHECK:       [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val2:%\d+]] = OpUConvert %ushort [[word1]]
// CHECK:          [[vec0:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val1]] [[val2]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[word1_shifted:%\d+]] = OpShiftRightLogical %uint [[word1]] %uint_16
// CHECK:          [[val3:%\d+]] = OpUConvert %ushort [[word1_shifted:%\d+]]
// CHECK:       [[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val4:%\d+]] = OpUConvert %ushort [[word2]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word2:%\d+]] = OpShiftRightLogical %uint [[word2]] %uint_16
// CHECK:          [[val5:%\d+]] = OpUConvert %ushort [[shifted_word2]]
// CHECK:       [[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:          [[vec1:%\d+]] = OpCompositeConstruct %v3ushort [[val3]] [[val4]] [[val5]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:         [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val6:%\d+]] = OpUConvert %ushort [[word3]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:         [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word3:%\d+]] = OpShiftRightLogical %uint [[word3]] %uint_16
// CHECK:          [[val7:%\d+]] = OpUConvert %ushort [[shifted_word3]]
// CHECK:       [[index_4:%\d+]] = OpIAdd %uint [[index_3]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:         [[word4:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val8:%\d+]] = OpUConvert %ushort [[word4]]
// CHECK:          [[vec2:%\d+]] = OpCompositeConstruct %v3ushort [[val6]] [[val7]] [[val8]]
// CHECK:        [[vecArr:%\d+]] = OpCompositeConstruct %_arr_v3ushort_uint_3 [[vec0]] [[vec1]] [[vec2]]
// CHECK:                          OpStore %uVec [[vecArr]]
  uint16_t3 uVec[3] = buf.Load<uint16_t3[3]>(tid.x);
}

