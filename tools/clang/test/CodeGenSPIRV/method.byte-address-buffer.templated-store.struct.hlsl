// Run: %dxc -T cs_6_2 -E main -enable-16bit-types -fvk-use-dx-layout

ByteAddressBuffer buf;
RWByteAddressBuffer buf2;

struct T {
  float16_t x[5];
};

struct S {
  float16_t3 a[3];
  double c;
  T t;
  double b;
  float16_t d;
};

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId) {
  S sArr[2] = buf.Load<S[2]>(tid.x);
  buf2.Store<S[2]>(tid.x, sArr);
}

// Initialization of sArr array.
// CHECK: OpStore %sArr {{%\d+}}
//
// Check for templated 'Store' method.
//
// CHECK:          [[tidx_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %tid %int_0
// CHECK:              [[tidx:%\d+]] = OpLoad %uint [[tidx_ptr]]
// CHECK:          [[address0:%\d+]] = OpShiftRightLogical %uint [[tidx]] %uint_2
// CHECK:              [[sArr:%\d+]] = OpLoad %_arr_S_uint_2 %sArr
// CHECK:                [[s0:%\d+]] = OpCompositeExtract %S [[sArr]] 0
// CHECK:                [[s1:%\d+]] = OpCompositeExtract %S [[sArr]] 1
// CHECK:                 [[a:%\d+]] = OpCompositeExtract %_arr_v3half_uint_3 [[s0]] 0
// CHECK:                [[a0:%\d+]] = OpCompositeExtract %v3half [[a]] 0
// CHECK:                [[a1:%\d+]] = OpCompositeExtract %v3half [[a]] 1
// CHECK:                [[a2:%\d+]] = OpCompositeExtract %v3half [[a]] 2
// CHECK:               [[a00:%\d+]] = OpCompositeExtract %half [[a0]] 0
// CHECK:               [[a01:%\d+]] = OpCompositeExtract %half [[a0]] 1
// CHECK:               [[a02:%\d+]] = OpCompositeExtract %half [[a0]] 2
// CHECK:               [[a10:%\d+]] = OpCompositeExtract %half [[a1]] 0
// CHECK:               [[a11:%\d+]] = OpCompositeExtract %half [[a1]] 1
// CHECK:               [[a12:%\d+]] = OpCompositeExtract %half [[a1]] 2
// CHECK:               [[a20:%\d+]] = OpCompositeExtract %half [[a2]] 0
// CHECK:               [[a21:%\d+]] = OpCompositeExtract %half [[a2]] 1
// CHECK:               [[a22:%\d+]] = OpCompositeExtract %half [[a2]] 2
// CHECK:         [[a00_16bit:%\d+]] = OpBitcast %ushort [[a00]]
// CHECK:         [[a00_32bit:%\d+]] = OpUConvert %uint [[a00_16bit]]
// CHECK:         [[a01_16bit:%\d+]] = OpBitcast %ushort [[a01]]
// CHECK:         [[a01_32bit:%\d+]] = OpUConvert %uint [[a01_16bit]]
// CHECK: [[a01_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a01_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a00_32bit]] [[a01_32bit_shifted]]
// CHECK:              [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address0]]
// CHECK:                             OpStore [[ptr]] [[word]]

// CHECK:          [[address1:%\d+]] = OpIAdd %uint [[address0]] %uint_1
// CHECK:         [[a02_16bit:%\d+]] = OpBitcast %ushort [[a02]]
// CHECK:         [[a02_32bit:%\d+]] = OpUConvert %uint [[a02_16bit]]
// CHECK:         [[a10_16bit:%\d+]] = OpBitcast %ushort [[a10]]
// CHECK:         [[a10_32bit:%\d+]] = OpUConvert %uint [[a10_16bit]]
// CHECK: [[a10_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a10_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a02_32bit]] [[a10_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address1]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:          [[address2:%\d+]] = OpIAdd %uint [[address1]] %uint_1
// CHECK:         [[a11_16bit:%\d+]] = OpBitcast %ushort [[a11]]
// CHECK:         [[a11_32bit:%\d+]] = OpUConvert %uint [[a11_16bit]]
// CHECK:         [[a12_16bit:%\d+]] = OpBitcast %ushort [[a12]]
// CHECK:         [[a12_32bit:%\d+]] = OpUConvert %uint [[a12_16bit]]
// CHECK: [[a12_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a12_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a11_32bit]] [[a12_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address2]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:          [[address3:%\d+]] = OpIAdd %uint [[address2]] %uint_1
// CHECK:         [[a20_16bit:%\d+]] = OpBitcast %ushort [[a20]]
// CHECK:         [[a20_32bit:%\d+]] = OpUConvert %uint [[a20_16bit]]
// CHECK:         [[a21_16bit:%\d+]] = OpBitcast %ushort [[a21]]
// CHECK:         [[a21_32bit:%\d+]] = OpUConvert %uint [[a21_16bit]]
// CHECK: [[a21_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a21_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a20_32bit]] [[a21_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address3]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:          [[address4:%\d+]] = OpIAdd %uint [[address3]] %uint_1
// CHECK:         [[a22_16bit:%\d+]] = OpBitcast %ushort [[a22]]
// CHECK:         [[a22_32bit:%\d+]] = OpUConvert %uint [[a22_16bit]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address4]]
// CHECK:                              OpStore [[ptr]] [[a22_32bit]]

//
// The second member of S starts at byte offset 24 (6 words)
//
// CHECK: [[address6:%\d+]] = OpIAdd %uint [[address0]] %uint_6
//
// CHECK:             [[c:%\d+]] = OpCompositeExtract %double [[s0]] 1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address6]]
// CHECK:         [[c_u64:%\d+]] = OpBitcast %ulong [[c]]
// CHECK:       [[c_word0:%\d+]] = OpUConvert %uint [[c_u64]]
// CHECK: [[c_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[c_u64]] %uint_32
// CHECK:       [[c_word1:%\d+]] = OpUConvert %uint [[c_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[c_word0]]
// CHECK:      [[address7:%\d+]] = OpIAdd %uint [[address6]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address7]]
// CHECK:                          OpStore [[ptr]] [[c_word1]]

//
// The third member of S starts at byte offset 32 (8 words)
//
// CHECK: [[address8:%\d+]] = OpIAdd %uint [[address0]] %uint_8
//
// CHECK:              [[t:%\d+]] = OpCompositeExtract %T [[s0]] 2
// CHECK:              [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[t]] 0
// CHECK:             [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:             [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:             [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:             [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:             [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:         [[x0_u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0_u32:%\d+]] = OpUConvert %uint [[x0_u16]]
// CHECK:         [[x1_u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1_u32:%\d+]] = OpUConvert %uint [[x1_u16]]
// CHECK: [[x1_u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x1_u32]] %uint_16
// CHECK:           [[word:%\d+]] = OpBitwiseOr %uint [[x0_u32]] [[x1_u32_shifted]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address8]]
// CHECK:                           OpStore [[ptr]] [[word]]
// CHECK:       [[address9:%\d+]] = OpIAdd %uint [[address8]] %uint_1
// CHECK:         [[x2_u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2_u32:%\d+]] = OpUConvert %uint [[x2_u16]]
// CHECK:         [[x3_u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3_u32:%\d+]] = OpUConvert %uint [[x3_u16:%\d+]]
// CHECK: [[x3_u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x3_u32]] %uint_16
// CHECK:           [[word:%\d+]] = OpBitwiseOr %uint [[x2_u32]] [[x3_u32_shifted]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address9]]
// CHECK:                           OpStore [[ptr]] [[word]]
// CHECK:      [[address10:%\d+]] = OpIAdd %uint [[address9]] %uint_1
// CHECK:         [[x4_u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4_u32:%\d+]] = OpUConvert %uint [[x4_u16]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address10]]
// CHECK:                           OpStore [[ptr]] [[x4_u32]]

//
// The fourth member of S starts at byte offset 48 (12 words)
//
// CHECK: [[address12:%\d+]] = OpIAdd %uint [[address0]] %uint_12
//
// CHECK:             [[b:%\d+]] = OpCompositeExtract %double [[s0]] 3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address12]]
// CHECK:         [[b_u64:%\d+]] = OpBitcast %ulong [[b]]
// CHECK:       [[b_word0:%\d+]] = OpUConvert %uint [[b_u64]]
// CHECK: [[b_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[b_u64]] %uint_32
// CHECK:       [[b_word1:%\d+]] = OpUConvert %uint [[b_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[b_word0]]
// CHECK:     [[address13:%\d+]] = OpIAdd %uint [[address12]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address13]]
// CHECK:                          OpStore [[ptr]] [[b_word1]]

//
// The fifth member of S starts at byte offset 56 (14 words)
//
// CHECK: [[address14:%\d+]] = OpIAdd %uint [[address0]] %uint_14
//
// CHECK:     [[d:%\d+]] = OpCompositeExtract %half [[s0]] 4
// CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address14]]
// CHECK: [[d_u16:%\d+]] = OpBitcast %ushort [[d]]
// CHECK: [[d_u32:%\d+]] = OpUConvert %uint [[d_u16]]
// CHECK:                  OpStore [[ptr]] [[d_u32]]

//
//
// We have an array of S structures (sArr). The second member (sArr[1]) should
// start at an aligned address. A structure aligment is the maximum alignment
// of its members.
// In this example, sArr[1] should start at byte offset 64 (16 words)
//
//
// CHECK: [[address16:%\d+]] = OpIAdd %uint [[address0]] %uint_16
//
// CHECK:                 [[a:%\d+]] = OpCompositeExtract %_arr_v3half_uint_3 [[s1]] 0
// CHECK:                [[a0:%\d+]] = OpCompositeExtract %v3half [[a]] 0
// CHECK:                [[a1:%\d+]] = OpCompositeExtract %v3half [[a]] 1
// CHECK:                [[a2:%\d+]] = OpCompositeExtract %v3half [[a]] 2
// CHECK:               [[a00:%\d+]] = OpCompositeExtract %half [[a0]] 0
// CHECK:               [[a01:%\d+]] = OpCompositeExtract %half [[a0]] 1
// CHECK:               [[a02:%\d+]] = OpCompositeExtract %half [[a0]] 2
// CHECK:               [[a10:%\d+]] = OpCompositeExtract %half [[a1]] 0
// CHECK:               [[a11:%\d+]] = OpCompositeExtract %half [[a1]] 1
// CHECK:               [[a12:%\d+]] = OpCompositeExtract %half [[a1]] 2
// CHECK:               [[a20:%\d+]] = OpCompositeExtract %half [[a2]] 0
// CHECK:               [[a21:%\d+]] = OpCompositeExtract %half [[a2]] 1
// CHECK:               [[a22:%\d+]] = OpCompositeExtract %half [[a2]] 2
// CHECK:         [[a00_16bit:%\d+]] = OpBitcast %ushort [[a00]]
// CHECK:         [[a00_32bit:%\d+]] = OpUConvert %uint [[a00_16bit]]
// CHECK:         [[a01_16bit:%\d+]] = OpBitcast %ushort [[a01]]
// CHECK:         [[a01_32bit:%\d+]] = OpUConvert %uint [[a01_16bit]]
// CHECK: [[a01_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a01_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a00_32bit]] [[a01_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address16]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address17:%\d+]] = OpIAdd %uint [[address16]] %uint_1
// CHECK:         [[a02_16bit:%\d+]] = OpBitcast %ushort [[a02]]
// CHECK:         [[a02_32bit:%\d+]] = OpUConvert %uint [[a02_16bit]]
// CHECK:         [[a10_16bit:%\d+]] = OpBitcast %ushort [[a10]]
// CHECK:         [[a10_32bit:%\d+]] = OpUConvert %uint [[a10_16bit]]
// CHECK: [[a10_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a10_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a02_32bit]] [[a10_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address17]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address18:%\d+]] = OpIAdd %uint [[address17]] %uint_1
// CHECK:         [[a11_16bit:%\d+]] = OpBitcast %ushort [[a11]]
// CHECK:         [[a11_32bit:%\d+]] = OpUConvert %uint [[a11_16bit]]
// CHECK:         [[a12_16bit:%\d+]] = OpBitcast %ushort [[a12]]
// CHECK:         [[a12_32bit:%\d+]] = OpUConvert %uint [[a12_16bit]]
// CHECK: [[a12_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a12_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a11_32bit]] [[a12_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address18]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address19:%\d+]] = OpIAdd %uint [[address18]] %uint_1
// CHECK:         [[a20_16bit:%\d+]] = OpBitcast %ushort [[a20]]
// CHECK:         [[a20_32bit:%\d+]] = OpUConvert %uint [[a20_16bit]]
// CHECK:         [[a21_16bit:%\d+]] = OpBitcast %ushort [[a21]]
// CHECK:         [[a21_32bit:%\d+]] = OpUConvert %uint [[a21_16bit]]
// CHECK: [[a21_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a21_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a20_32bit]] [[a21_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address19]]
// CHECK:                              OpStore [[ptr]] [[word]]
// CHECK:         [[address20:%\d+]] = OpIAdd %uint [[address19]] %uint_1
// CHECK:         [[a22_16bit:%\d+]] = OpBitcast %ushort [[a22]]
// CHECK:         [[a22_32bit:%\d+]] = OpUConvert %uint [[a22_16bit]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address20]]
// CHECK:                              OpStore [[ptr]] [[a22_32bit]]

//
// The second member of S starts at byte offset 24 (6 words)
//
// CHECK: [[address22:%\d+]] = OpIAdd %uint [[address16]] %uint_6
//
// CHECK:             [[c:%\d+]] = OpCompositeExtract %double [[s1]] 1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address22]]
// CHECK:         [[c_u64:%\d+]] = OpBitcast %ulong [[c]]
// CHECK:       [[c_word0:%\d+]] = OpUConvert %uint [[c_u64]]
// CHECK: [[c_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[c_u64]] %uint_32
// CHECK:       [[c_word1:%\d+]] = OpUConvert %uint [[c_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[c_word0]]
// CHECK:     [[address23:%\d+]] = OpIAdd %uint [[address22]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address23]]
// CHECK:                          OpStore [[ptr]] [[c_word1]]

//
// The third member of S starts at byte offset 32 (8 words)
//
// CHECK: [[address24:%\d+]] = OpIAdd %uint [[address16]] %uint_8
//
// CHECK:              [[t:%\d+]] = OpCompositeExtract %T [[s1]] 2
// CHECK:              [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[t]] 0
// CHECK:             [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:             [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:             [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:             [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:             [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:         [[x0_u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0_u32:%\d+]] = OpUConvert %uint [[x0_u16]]
// CHECK:         [[x1_u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1_u32:%\d+]] = OpUConvert %uint [[x1_u16]]
// CHECK: [[x1_u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x1_u32]] %uint_16
// CHECK:           [[word:%\d+]] = OpBitwiseOr %uint [[x0_u32]] [[x1_u32_shifted]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address24]]
// CHECK:                           OpStore [[ptr]] [[word]]

// CHECK:      [[address25:%\d+]] = OpIAdd %uint [[address24]] %uint_1
// CHECK:         [[x2_u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2_u32:%\d+]] = OpUConvert %uint [[x2_u16]]
// CHECK:         [[x3_u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3_u32:%\d+]] = OpUConvert %uint [[x3_u16:%\d+]]
// CHECK: [[x3_u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x3_u32]] %uint_16
// CHECK:           [[word:%\d+]] = OpBitwiseOr %uint [[x2_u32]] [[x3_u32_shifted]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address25]]
// CHECK:                           OpStore [[ptr]] [[word]]

// CHECK:      [[address26:%\d+]] = OpIAdd %uint [[address25]] %uint_1
// CHECK:         [[x4_u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4_u32:%\d+]] = OpUConvert %uint [[x4_u16]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address26]]
// CHECK:                           OpStore [[ptr]] [[x4_u32]]

//
// The fourth member of S starts at byte offset 48 (12 words)
//
// CHECK: [[address28:%\d+]] = OpIAdd %uint [[address16]] %uint_12
//
// CHECK:             [[b:%\d+]] = OpCompositeExtract %double [[s1]] 3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address28]]
// CHECK:         [[b_u64:%\d+]] = OpBitcast %ulong [[b]]
// CHECK:       [[b_word0:%\d+]] = OpUConvert %uint [[b_u64]]
// CHECK: [[b_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[b_u64]] %uint_32
// CHECK:       [[b_word1:%\d+]] = OpUConvert %uint [[b_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[b_word0]]
// CHECK:     [[address29:%\d+]] = OpIAdd %uint [[address28]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address29]]
// CHECK:                          OpStore [[ptr]] [[b_word1]]

//
// The fifth member of S starts at byte offset 56 (14 words)
//
// CHECK: [[address30:%\d+]] = OpIAdd %uint [[address16]] %uint_14
//
// CHECK:     [[d:%\d+]] = OpCompositeExtract %half [[s1]] 4
// CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address30]]
// CHECK: [[d_u16:%\d+]] = OpBitcast %ushort [[d]]
// CHECK: [[d_u32:%\d+]] = OpUConvert %uint [[d_u16]]
// CHECK:                  OpStore [[ptr]] [[d_u32]]

