// Run: %dxc -T cs_6_2 -E main -enable-16bit-types

ByteAddressBuffer buf;

[numthreads(64, 1, 1)] void main(uint3 tid
                                 : SV_DispatchThreadId) {
  // ********* 16-bit scalar ********************

  // CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK:   [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[ushort:%\d+]] = OpUConvert %ushort [[uint]]
  // CHECK:                   OpStore %u16 [[ushort]]
  uint16_t u16 = buf.Load<uint16_t>(tid.x);

  // CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK:   [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[ushort:%\d+]] = OpUConvert %ushort [[uint]]
  // CHECK:  [[short:%\d+]] = OpBitcast %short [[ushort]]
  // CHECK:                   OpStore %i16 [[short]]
  int16_t i16 = buf.Load<int16_t>(tid.x);

  // CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK:   [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[ushort:%\d+]] = OpUConvert %ushort [[uint]]
  // CHECK:   [[half:%\d+]] = OpBitcast %half [[ushort]]
  // CHECK:                   OpStore %f16 [[half]]
  float16_t f16 = buf.Load<float16_t>(tid.x);

  // ********* 32-bit scalar ********************

  // CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK:                 OpStore %u [[uint]]
  uint u = buf.Load<uint>(tid.x);

  // CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr:%\d+]]
  // CHECK:  [[int:%\d+]] = OpBitcast %int [[uint]]
  // CHECK:                 OpStore %i [[int]]
  int i = buf.Load<int>(tid.x);

  // CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK:  [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[float:%\d+]] = OpBitcast %float [[uint]]
  // CHECK:                  OpStore %f [[float]]
  float f = buf.Load<float>(tid.x);

  // CHECK:  [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 {{%\d+}}
  // CHECK: [[uint:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
  // CHECK:                 OpStore %b [[bool]]
  bool b = buf.Load<bool>(tid.x);

  // ********* 64-bit scalar ********************

// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr:%\d+]]
// CHECK:            [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[newAddr:%\d+]] = OpIAdd %uint [[addr]] %uint_1
// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[newAddr]]
// CHECK:            [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:       [[word0ULong:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:       [[word1ULong:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK:[[shiftedWord1ULong:%\d+]] = OpShiftLeftLogical %ulong [[word1ULong]] %uint_32
// CHECK:              [[val:%\d+]] = OpBitwiseOr %ulong [[word0ULong]] [[shiftedWord1ULong]]
// CHECK:                             OpStore %u64 [[val]]
  uint64_t u64 = buf.Load<uint64_t>(tid.x);

// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr:%\d+]]
// CHECK:            [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[newAddr:%\d+]] = OpIAdd %uint [[addr]] %uint_1
// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[newAddr]]
// CHECK:            [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:        [[word0Long:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:        [[word1Long:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK: [[shiftedWord1Long:%\d+]] = OpShiftLeftLogical %ulong [[word1Long]] %uint_32
// CHECK:        [[val_ulong:%\d+]] = OpBitwiseOr %ulong [[word0Long]] [[shiftedWord1Long]]
// CHECK:         [[val_long:%\d+]] = OpBitcast %long [[val_ulong]]
// CHECK:                             OpStore %i64 [[val_long]]
  int64_t i64 = buf.Load<int64_t>(tid.x);

// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr:%\d+]]
// CHECK:            [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:          [[newAddr:%\d+]] = OpIAdd %uint [[addr]] %uint_1
// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[newAddr]]
// CHECK:            [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:        [[word0Long:%\d+]] = OpUConvert %ulong [[word0]]
// CHECK:        [[word1Long:%\d+]] = OpUConvert %ulong [[word1]]
// CHECK: [[shiftedWord1Long:%\d+]] = OpShiftLeftLogical %ulong [[word1Long]] %uint_32
// CHECK:        [[val_ulong:%\d+]] = OpBitwiseOr %ulong [[word0Long]] [[shiftedWord1Long]]
// CHECK:       [[val_double:%\d+]] = OpBitcast %double [[val_ulong]]
// CHECK:                             OpStore %f64 [[val_double]]
  double f64 = buf.Load<double>(tid.x);

  // ********* array of scalars *****************

// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr:%\d+]]
// CHECK:    [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[val0:%\d+]] = OpUConvert %ushort [[word0]]
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr]]
// CHECK:    [[word0:%\d+]] = OpLoad %uint [[ptr]]
// CHECK: [[val1uint:%\d+]] = OpShiftRightLogical %uint [[word0]] %uint_16
// CHECK:     [[val1:%\d+]] = OpUConvert %ushort [[val1uint]]
// CHECK:  [[newAddr:%\d+]] = OpIAdd %uint [[addr]] %uint_1
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[newAddr]]
// CHECK:    [[word1:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[val2:%\d+]] = OpUConvert %ushort [[word1]]
// CHECK:     [[uArr:%\d+]] = OpCompositeConstruct %_arr_ushort_uint_3 [[val0]] [[val1]] [[val2]]
// CHECK:                     OpStore %uArr [[uArr]]
  uint16_t uArr[3] = buf.Load<uint16_t[3]>(tid.x);

// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr:%\d+]]
// CHECK: [[val0_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:      [[val0:%\d+]] = OpBitcast %int [[val0_uint]]
// CHECK:   [[newAddr:%\d+]] = OpIAdd %uint [[addr]] %uint_1
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[newAddr]]
// CHECK: [[val1_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:      [[val1:%\d+]] = OpBitcast %int [[val1_uint]]
// CHECK:      [[iArr:%\d+]] = OpCompositeConstruct %_arr_int_uint_2 [[val0]] [[val1]]
// CHECK:                      OpStore %iArr [[iArr]]
  int iArr[2] = buf.Load<int[2]>(tid.x);

// CHECK:                      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr_0:%\d+]]
// CHECK:          [[val0_word0_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:                   [[addr_1:%\d+]] = OpIAdd %uint [[addr_0]] %uint_1
// CHECK:                      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr_1]]
// CHECK:          [[val0_word1_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[val0_word0_ulong:%\d+]] = OpUConvert %ulong [[val0_word0_uint]]
// CHECK:         [[val0_word1_ulong:%\d+]] = OpUConvert %ulong [[val0_word1_uint]]
// CHECK: [[shifted_val0_word1_ulong:%\d+]] = OpShiftLeftLogical %ulong [[val0_word1_ulong]] %uint_32
// CHECK:               [[val0_ulong:%\d+]] = OpBitwiseOr %ulong [[val0_word0_ulong]] [[shifted_val0_word1_ulong]]
// CHECK:              [[val0_double:%\d+]] = OpBitcast %double [[val0_ulong]]
//
// CHECK:                   [[addr_2:%\d+]] = OpIAdd %uint [[addr_1]] %uint_1
// CHECK:                      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr_2]]
// CHECK:          [[val1_word0_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:                   [[addr_3:%\d+]] = OpIAdd %uint [[addr_2]] %uint_1
// CHECK:                      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[addr_3]]
// CHECK:          [[val1_word1_uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:         [[val1_word0_ulong:%\d+]] = OpUConvert %ulong [[val1_word0_uint]]
// CHECK:         [[val1_word1_ulong:%\d+]] = OpUConvert %ulong [[val1_word1_uint]]
// CHECK: [[shifted_val1_word1_ulong:%\d+]] = OpShiftLeftLogical %ulong [[val1_word1_ulong]] %uint_32
// CHECK:               [[val1_ulong:%\d+]] = OpBitwiseOr %ulong [[val1_word0_ulong]] [[shifted_val1_word1_ulong]]
// CHECK:              [[val1_double:%\d+]] = OpBitcast %double [[val1_ulong]]
//
// CHECK:                     [[fArr:%\d+]] = OpCompositeConstruct %_arr_double_uint_2 [[val0_double]] [[val1_double]]
// CHECK:                                     OpStore %fArr [[fArr]]
  double fArr[2] = buf.Load<double[2]>(tid.x);
}

