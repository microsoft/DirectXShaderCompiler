// Run: %dxc -T cs_6_2 -E main -enable-16bit-types

ByteAddressBuffer buf;

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId)
{
// ********* 16-bit matrix ********************

// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index:%\d+]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val0:%\d+]] = OpUConvert %ushort [[word0]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word0:%\d+]] = OpShiftRightLogical %uint [[word0]] %uint_16
// CHECK:          [[val1:%\d+]] = OpUConvert %ushort [[shifted_word0]]
// CHECK:       [[index_1:%\d+]] = OpIAdd %uint [[index]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val2:%\d+]] = OpUConvert %ushort [[word1]]
// CHECK:          [[row0:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val1]] [[val2]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word1:%\d+]] = OpShiftRightLogical %uint [[word1]] %uint_16
// CHECK:          [[val3:%\d+]] = OpUConvert %ushort [[shifted_word1:%\d+]]
// CHECK:       [[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[val4:%\d+]] = OpUConvert %ushort [[word2:%\d+]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word2:%\d+]] = OpShiftRightLogical %uint [[word2]] %uint_16
// CHECK:          [[val5:%\d+]] = OpUConvert %ushort [[shifted_word2:%\d+]]
// CHECK:          [[row1:%\d+]] = OpCompositeConstruct %v3ushort [[val3]] [[val4]] [[val5]]
// CHECK:        [[matrix:%\d+]] = OpCompositeConstruct %_arr_v3ushort_uint_2 [[row0]] [[row1]]
// CHECK:                          OpStore %u16 [[matrix]]
  uint16_t2x3 u16 = buf.Load<uint16_t2x3>(tid.x);


// ********* 32-bit matrix ********************

// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:  [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val0:%\d+]] = OpBitcast %int [[word0]]
// CHECK:[[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:  [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val1:%\d+]] = OpBitcast %int [[word1:%\d+]]
// CHECK:[[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:   [[row0:%\d+]] = OpCompositeConstruct %v2int [[val0]] [[val1]]
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:  [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val2:%\d+]] = OpBitcast %int [[word2]]
// CHECK:[[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:  [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val3:%\d+]] = OpBitcast %int [[word3]]
// CHECK:   [[row1:%\d+]] = OpCompositeConstruct %v2int [[val2]] [[val3]]
// CHECK: [[matrix:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_2 [[row0]] [[row1]]
// CHECK:                   OpStore %i [[matrix]]
  int2x2 i = buf.Load<int2x2>(tid.x);

// ********* 64-bit matrix ********************

// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:               [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:             [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:               [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[word0_ulong:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:         [[word1_ulong:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK: [[word1_ulong_shifted:%\d+]] = OpShiftLeftLogical %ulong [[word1_ulong]] %uint_32
// CHECK:          [[val0_ulong:%\d+]] = OpBitwiseOr %ulong [[word0_ulong]] [[word1_ulong_shifted]]
// CHECK:                [[val0:%\d+]] = OpBitcast %double [[val0_ulong]]
// CHECK:             [[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:               [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:             [[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:               [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[word2_ulong:%\d+]] = OpUConvert %ulong [[word2]]
// CHECK:         [[word3_ulong:%\d+]] = OpUConvert %ulong [[word3]]
// CHECK: [[word3_ulong_shifted:%\d+]] = OpShiftLeftLogical %ulong [[word3_ulong]] %uint_32
// CHECK:          [[val1_ulong:%\d+]] = OpBitwiseOr %ulong [[word2_ulong]] [[word3_ulong_shifted]]
// CHECK:                [[val1:%\d+]] = OpBitcast %double [[val1_ulong]]
// CHECK:             [[index_4:%\d+]] = OpIAdd %uint [[index_3]] %uint_1
// CHECK:                [[row0:%\d+]] = OpCompositeConstruct %v2double [[val0]] [[val1]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:               [[word4:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:             [[index_5:%\d+]] = OpIAdd %uint [[index_4]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:               [[word5:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[word4_ulong:%\d+]] = OpUConvert %ulong [[word4]]
// CHECK:         [[word5_ulong:%\d+]] = OpUConvert %ulong [[word5]]
// CHECK: [[word5_ulong_shifted:%\d+]] = OpShiftLeftLogical %ulong [[word5_ulong]] %uint_32
// CHECK:          [[val2_ulong:%\d+]] = OpBitwiseOr %ulong [[word4_ulong]] [[word5_ulong_shifted]]
// CHECK:                [[val2:%\d+]] = OpBitcast %double [[val2_ulong]]
// CHECK:             [[index_6:%\d+]] = OpIAdd %uint [[index_5]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_6]]
// CHECK:               [[word6:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:             [[index_7:%\d+]] = OpIAdd %uint [[index_6]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_7]]
// CHECK:               [[word7:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[word6_ulong:%\d+]] = OpUConvert %ulong [[word6]]
// CHECK:         [[word7_ulong:%\d+]] = OpUConvert %ulong [[word7]]
// CHECK: [[word7_ulong_shifted:%\d+]] = OpShiftLeftLogical %ulong [[word7_ulong]] %uint_32
// CHECK:          [[val3_ulong:%\d+]] = OpBitwiseOr %ulong [[word6_ulong]] [[word7_ulong_shifted]]
// CHECK:                [[val3:%\d+]] = OpBitcast %double [[val3_ulong]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v2double [[val2]] [[val3]]
// CHECK:              [[matrix:%\d+]] = OpCompositeConstruct %mat2v2double [[row0]] [[row1]]
// CHECK:                                OpStore %f64 [[matrix]]
  float64_t2x2 f64 = buf.Load<float64_t2x2>(tid.x);

// ********* array of matrices ********************

// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0:%\d+]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:             [[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:             [[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:             [[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_1:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:             [[index_4:%\d+]] = OpIAdd %uint [[index_3]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:             [[index_5:%\d+]] = OpIAdd %uint [[index_4]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:             [[index_6:%\d+]] = OpIAdd %uint [[index_5]] %uint_1
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_2:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_6]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_6]]
// CHECK:             [[index_7:%\d+]] = OpIAdd %uint [[index_6]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_7]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_7]]
// CHECK:             [[index_8:%\d+]] = OpIAdd %uint [[index_7]] %uint_1
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_8]]
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_8]]
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_3:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:        [[matrix_array:%\d+]] = OpCompositeConstruct %_arr_mat2v3half_uint_3 [[matrix_1]] [[matrix_2]] [[matrix_3]]
// CHECK:                                OpStore %matVec [[matrix_array]]
  float16_t2x3 matVec[3] = buf.Load<float16_t2x3[3]>(tid.x);
}

