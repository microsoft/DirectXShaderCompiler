// RUN: %dxc -T cs_6_2 -E main -enable-16bit-types

void foo(float16_t2x3 param[3]) {}

ByteAddressBuffer buf;

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId)
{
// ********* 16-bit matrix ********************

// CHECK:         [[index:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr0]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word0:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff]]
// CHECK:          [[val0:%\d+]] = OpUConvert %ushort [[shifted_word0]]
// CHECK:         [[addr1:%\d+]] = OpIAdd %uint [[addr0]] %uint_2
// CHECK:         [[index:%\d+]] = OpShiftRightLogical %uint [[addr1]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr1]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word0:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOff]]
// CHECK:          [[val1:%\d+]] = OpUConvert %ushort [[shifted_word0]]
// CHECK:         [[addr2:%\d+]] = OpIAdd %uint [[addr1]] %uint_2
// CHECK:       [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr2]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr2]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word1:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOff]]
// CHECK:          [[val2:%\d+]] = OpUConvert %ushort [[shifted_word1]]
// CHECK:         [[addr3:%\d+]] = OpIAdd %uint [[addr2]] %uint_2
// CHECK:       [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr3]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr3]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word1:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOff]]
// CHECK:          [[val3:%\d+]] = OpUConvert %ushort [[shifted_word1:%\d+]]
// CHECK:         [[addr4:%\d+]] = OpIAdd %uint [[addr3]] %uint_2
// CHECK:       [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr4]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr4]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word2:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOff]]
// CHECK:          [[val4:%\d+]] = OpUConvert %ushort [[shifted_word2:%\d+]]
// CHECK:         [[addr5:%\d+]] = OpIAdd %uint [[addr4]] %uint_2
// CHECK:       [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr5]] %uint_2
// CHECK:       [[byteOff:%\d+]] = OpUMod %uint [[addr5]] %uint_4
// CHECK:        [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[shifted_word2:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOff]]
// CHECK:          [[val5:%\d+]] = OpUConvert %ushort [[shifted_word2:%\d+]]
// CHECK:          [[row0:%\d+]] = OpCompositeConstruct %v3ushort [[val0]] [[val2]] [[val4]]
// CHECK:          [[row1:%\d+]] = OpCompositeConstruct %v3ushort [[val1]] [[val3]] [[val5]]
// CHECK:        [[matrix:%\d+]] = OpCompositeConstruct %_arr_v3ushort_uint_2 [[row0]] [[row1]]
// CHECK:                          OpStore %u16 [[matrix]]
  uint16_t2x3 u16 = buf.Load<uint16_t2x3>(tid.x);

// ********* 32-bit matrix ********************

// CHECK:[[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:  [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val0:%\d+]] = OpBitcast %int [[word0]]
// CHECK:[[index_1:%\d+]] = OpIAdd %uint [[index_0]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:  [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val1:%\d+]] = OpBitcast %int [[word1:%\d+]]
// CHECK:[[index_2:%\d+]] = OpIAdd %uint [[index_1]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:  [[word2:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val2:%\d+]] = OpBitcast %int [[word2]]
// CHECK:[[index_3:%\d+]] = OpIAdd %uint [[index_2]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:  [[word3:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val3:%\d+]] = OpBitcast %int [[word3]]
// CHECK:[[index_4:%\d+]] = OpIAdd %uint [[index_3]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:  [[word4:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val4:%\d+]] = OpBitcast %int [[word4]]
// CHECK:[[index_5:%\d+]] = OpIAdd %uint [[index_4]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:  [[word5:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:   [[val5:%\d+]] = OpBitcast %int [[word5]]
// CHECK:   [[row0:%\d+]] = OpCompositeConstruct %v2int [[val0]] [[val3]]
// CHECK:   [[row1:%\d+]] = OpCompositeConstruct %v2int [[val1]] [[val4]]
// CHECK:   [[row2:%\d+]] = OpCompositeConstruct %v2int [[val2]] [[val5]]
// CHECK: [[matrix:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[row0]] [[row1]] [[row2]]
// CHECK:                   OpStore %j [[matrix]]
  int3x2 j = buf.Load<int3x2>(tid.x);

// ********* 64-bit matrix ********************

// CHECK:             [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
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
// CHECK:                [[row0:%\d+]] = OpCompositeConstruct %v2double [[val0]] [[val2]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v2double [[val1]] [[val3]]
// CHECK:              [[matrix:%\d+]] = OpCompositeConstruct %mat2v2double [[row0]] [[row1]]
// CHECK:                                OpStore %f64 [[matrix]]
  float64_t2x2 f64 = buf.Load<float64_t2x2>(tid.x);

// ********* array of matrices ********************

// CHECK:             [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:               [[addr1:%\d+]] = OpIAdd %uint [[addr0]] %uint_2
// CHECK:             [[index_0:%\d+]] = OpShiftRightLogical %uint [[addr1]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_0]]
// CHECK:               [[addr2:%\d+]] = OpIAdd %uint [[addr1]] %uint_2
// CHECK:             [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr2]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:               [[addr3:%\d+]] = OpIAdd %uint [[addr2]] %uint_2
// CHECK:             [[index_1:%\d+]] = OpShiftRightLogical %uint [[addr3]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_1]]
// CHECK:               [[addr4:%\d+]] = OpIAdd %uint [[addr3]] %uint_2
// CHECK:             [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr4]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:               [[addr5:%\d+]] = OpIAdd %uint [[addr4]] %uint_2
// CHECK:             [[index_2:%\d+]] = OpShiftRightLogical %uint [[addr5]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_2]]
// CHECK:               [[addr6:%\d+]] = OpIAdd %uint [[addr5]] %uint_2
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_1:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:             [[index_3:%\d+]] = OpShiftRightLogical %uint [[addr6]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:               [[addr7:%\d+]] = OpIAdd %uint [[addr6]] %uint_2
// CHECK:             [[index_3:%\d+]] = OpShiftRightLogical %uint [[addr7]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_3]]
// CHECK:               [[addr8:%\d+]] = OpIAdd %uint [[addr7]] %uint_2
// CHECK:             [[index_4:%\d+]] = OpShiftRightLogical %uint [[addr8]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:               [[addr9:%\d+]] = OpIAdd %uint [[addr8]] %uint_2
// CHECK:             [[index_4:%\d+]] = OpShiftRightLogical %uint [[addr9]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_4]]
// CHECK:              [[addr10:%\d+]] = OpIAdd %uint [[addr9]] %uint_2
// CHECK:             [[index_5:%\d+]] = OpShiftRightLogical %uint [[addr10]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:              [[addr11:%\d+]] = OpIAdd %uint [[addr10]] %uint_2
// CHECK:             [[index_5:%\d+]] = OpShiftRightLogical %uint [[addr11]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_5]]
// CHECK:              [[addr12:%\d+]] = OpIAdd %uint [[addr11]] %uint_2
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_2:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:             [[index_6:%\d+]] = OpShiftRightLogical %uint [[addr12]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_6]]
// CHECK:              [[addr13:%\d+]] = OpIAdd %uint [[addr12]] %uint_2
// CHECK:             [[index_6:%\d+]] = OpShiftRightLogical %uint [[addr13]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_6]]
// CHECK:              [[addr14:%\d+]] = OpIAdd %uint [[addr13]] %uint_2
// CHECK:             [[index_7:%\d+]] = OpShiftRightLogical %uint [[addr14]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_7]]
// CHECK:              [[addr15:%\d+]] = OpIAdd %uint [[addr14]] %uint_2
// CHECK:             [[index_7:%\d+]] = OpShiftRightLogical %uint [[addr15]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_7]]
// CHECK:              [[addr16:%\d+]] = OpIAdd %uint [[addr15]] %uint_2
// CHECK:             [[index_8:%\d+]] = OpShiftRightLogical %uint [[addr16]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_8]]
// CHECK:              [[addr17:%\d+]] = OpIAdd %uint [[addr16]] %uint_2
// CHECK:             [[index_8:%\d+]] = OpShiftRightLogical %uint [[addr17]] %uint_2
// CHECK:                 [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[index_8]]
// CHECK:                [[row1:%\d+]] = OpCompositeConstruct %v3half
// CHECK:                [[row2:%\d+]] = OpCompositeConstruct %v3half
// CHECK:            [[matrix_3:%\d+]] = OpCompositeConstruct %mat2v3half [[row1]] [[row2]]
// CHECK:        [[matrix_array:%\d+]] = OpCompositeConstruct %_arr_mat2v3half_uint_3 [[matrix_1]] [[matrix_2]] [[matrix_3]]
// CHECK:                                OpStore %matVec [[matrix_array]]
  float16_t2x3 matVec[3] = buf.Load<float16_t2x3[3]>(tid.x);

//
// Check that the rvalue resulting from the templated load is accessed correctly
// A temporary LValue has to be constructed and accessed in order to do this.
//
// CHECK: OpCompositeConstruct %_arr_mat2v3half_uint_3
// CHECK: OpStore %temp_var_
// CHECK: OpAccessChain %_ptr_Function_mat2v3half %temp_var_ %int_0
// CHECK: OpLoad %mat2v3half
// CHECK: OpCompositeExtract %half {{%\d+}} 0 1
// CHECK: OpCompositeExtract %half {{%\d+}} 0 2
// CHECK: OpCompositeConstruct %v2half
// CHECK: OpStore %customMatrix {{%\d+}}
  float16_t2 customMatrix = (buf.Load<float16_t2x3[3]>(tid.x))[0]._m01_m02;

// CHECK: OpCompositeConstruct %_arr_mat2v3half_uint_3
// CHECK: OpStore %temp_var_vector
// CHECK: OpAccessChain %_ptr_Function_half %temp_var_vector %int_1 %uint_0 %uint_1
// CHECK: OpLoad %half
// CHECK: OpCompositeConstruct %_arr_half_uint_3
// CHECK: OpStore %a {{%\d+}}
  half a[3] = {1, (buf.Load<float16_t2x3[3]>(tid.x))[1][0][1], 0};

// CHECK: OpCompositeConstruct %_arr_mat2v3half_uint_3
// CHECK: OpStore %param_var_param {{%\d+}}
// CHECK: OpFunctionCall %void %foo %param_var_param
  foo(buf.Load<float16_t2x3[3]>(tid.x));
}
