// Run: %dxc -T cs_6_2 -E main -enable-16bit-types -fvk-use-dx-layout

ByteAddressBuffer buf;
RWByteAddressBuffer buf2;

struct T {
  float16_t x[5];
};

struct U {
  float16_t v[3];
  uint w;
};

struct S {
  float16_t3 a[3];
  double c;
  T t;
  double b;
  float16_t d;
  T e[2];
  U f[2];
  float16_t z;
};

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId) {
  S sArr[2] = buf.Load<S[2]>(tid.x);
  buf2.Store<S[2]>(tid.x, sArr);
}

// Note: the following indeces are taken from them DXIL compilation:
//
//
//                           // sArr[0] starts
//
//  %3 = 0                    // a[0] starts at byte offset 0
//  %8 = add i32 %3, 6        // a[1] starts at byte offset 6
// %13 = add i32 %3, 12       // a[2] starts at byte offset 12
//                            // since the next member is a 'double' it does not
//                            // start at offset 18 or 20. It starts at offset 24.
//                            // byte [18-23] inclusive are PADDING.
// %18 = add i32 %3, 24       // c starts at offset 24 (6 words)
// %23 = add i32 %3, 32       // t.x[0] starts at byte offset 32 (8 words)
// %26 = add i32 %3, 34       // t.x[1] starts at byte offset 34
// %29 = add i32 %3, 36       // t.x[2] starts at byte offset 36
// %32 = add i32 %3, 38       // t.x[2] starts at byte offset 38
// %35 = add i32 %3, 40       // t.x[2] starts at byte offset 40 
//                            // byte [42-47] inclusive are PADDING.
// %38 = add i32 %3, 48       // b starts at byte offset 48 (12 words)
// %43 = add i32 %3, 56       // d starts at byte offset 56 (14 words)
//                            // even though 'e' is the next struct member,
//                            // it does NOT start at an aligned address (does not start at 64 byte offset).
// %46 = add i32 %3, 58       // e[0].x[0] starts at byte offset 58
// %49 = add i32 %3, 60       // e[0].x[1] starts at byte offset 60
// %52 = add i32 %3, 62       // e[0].x[2] starts at byte offset 62
// %55 = add i32 %3, 64       // e[0].x[3] starts at byte offset 64
// %58 = add i32 %3, 66       // e[0].x[4] starts at byte offset 66
// %61 = add i32 %3, 68       // e[1].x[0] starts at byte offset 68
// %64 = add i32 %3, 70       // e[1].x[1] starts at byte offset 70
// %67 = add i32 %3, 72       // e[1].x[2] starts at byte offset 72
// %70 = add i32 %3, 74       // e[1].x[3] starts at byte offset 74
// %73 = add i32 %3, 76       // e[1].x[4] starts at byte offset 76
//                            // 'f' starts at the next aligned address
//                            // byte [78-79] inclusive are PADDING
// %76 = add i32 %3, 80       // f[0].v[0] starts at byte offset 80 (20 words)
// %79 = add i32 %3, 82       // f[0].v[1] starts at byte offset 82
// %82 = add i32 %3, 84       // f[0].v[2] starts at byte offset 84
//                            // byte [86-87] inclusive are PADDING
// %85 = add i32 %3, 88       // f[0].w starts at byte offset 88 (22 words)
// %88 = add i32 %3, 92       // f[1].v[0] starts at byte offset 92
// %91 = add i32 %3, 94       // f[1].v[1] starts at byte offset 94
// %94 = add i32 %3, 96       // f[1].v[2] starts at byte offset 96
//                            // byte [98-99] inclusive are PADDING
// %97 = add i32 %3, 100      // f[1].w starts at byte offset 100 (25 words)
// %100 = add i32 %3, 104     // z starts at byte offset 104 (26 words)
//
//                           // sArr[1] starts
//
//                           // byte [106-111] inclusive are PADDING
//
//                           // ALL the following offsets are similar to offsets
//                           // of sArr[0], shifted by 112 bytes.
//
// %103 = add i32 %3, 112
// %108 = add i32 %3, 118
// %113 = add i32 %3, 124
// %118 = add i32 %3, 136
// %123 = add i32 %3, 144
// %126 = add i32 %3, 146
// %129 = add i32 %3, 148
// %132 = add i32 %3, 150
// %135 = add i32 %3, 152
// %138 = add i32 %3, 160
// %143 = add i32 %3, 168
// %146 = add i32 %3, 170
// %149 = add i32 %3, 172
// %152 = add i32 %3, 174
// %155 = add i32 %3, 176
// %158 = add i32 %3, 178
// %161 = add i32 %3, 180
// %164 = add i32 %3, 182
// %167 = add i32 %3, 184
// %170 = add i32 %3, 186
// %173 = add i32 %3, 188
// %176 = add i32 %3, 192
// %179 = add i32 %3, 194
// %182 = add i32 %3, 196
// %185 = add i32 %3, 200
// %188 = add i32 %3, 204
// %191 = add i32 %3, 206
// %194 = add i32 %3, 208
// %197 = add i32 %3, 212
// %200 = add i32 %3, 216

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
// The sixth member of S starts at byte offset 58 (14 words + 16bit offset)
// This is an extraordinary case of alignment. Since the sixth member only
// contains fp16, and the fifth member was also fp16, DX packs them tightly.
// As a result, store must occur at non-aligned offset.
// e[0] takes the following byte offsets: 58, 60, 62, 64, 66.
// e[1] takes the following byte offsets: 68, 70, 72, 74, 76.
// (60-64 = index 15. 64-68 = index 16)
// (68-72 = index 17. 72-76 = index 18)
// (76-78 = first half of index 19)
//
// CHECK:     [[address14:%\d+]] = OpIAdd %uint [[address0]] %uint_14
// CHECK:             [[e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[s0]] 5
// CHECK:            [[e0:%\d+]] = OpCompositeExtract %T [[e]] 0
// CHECK:            [[e1:%\d+]] = OpCompositeExtract %T [[e]] 1
// CHECK:             [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[e0]] 0
// CHECK:            [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:            [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:            [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:            [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:            [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address14]]
// CHECK:         [[x0u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0u32:%\d+]] = OpUConvert %uint [[x0u16]]
// CHECK: [[x0u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x0u32]] %uint_16
// CHECK:  [[existingWord:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:       [[newWord:%\d+]] = OpBitwiseOr %uint [[existingWord]] [[x0u32_shifted]]
// CHECK:                          OpStore [[ptr]] [[newWord]]

// CHECK:     [[address15:%\d+]] = OpIAdd %uint [[address14]] %uint_1
// CHECK:         [[x1u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1u32:%\d+]] = OpUConvert %uint [[x1u16]]
// CHECK:         [[x2u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2u32:%\d+]] = OpUConvert %uint [[x2u16]]
// CHECK: [[x2u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x2u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x1u32]] [[x2u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address15]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address16:%\d+]] = OpIAdd %uint [[address15]] %uint_1
// CHECK:         [[x3u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3u32:%\d+]] = OpUConvert %uint [[x3u16]]
// CHECK:         [[x4u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4u32:%\d+]] = OpUConvert %uint [[x4u16]]
// CHECK: [[x4u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x4u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x3u32]] [[x4u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address16]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address17:%\d+]] = OpIAdd %uint [[address14]] %uint_3
// CHECK:             [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[e1]] 0
// CHECK:            [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:            [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:            [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:            [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:            [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:         [[x0u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0u32:%\d+]] = OpUConvert %uint [[x0u16]]
// CHECK:         [[x1u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1u32:%\d+]] = OpUConvert %uint [[x1u16]]
// CHECK: [[x1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x0u32]] [[x1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address17]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address18:%\d+]] = OpIAdd %uint [[address17]] %uint_1
// CHECK:         [[x2u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2u32:%\d+]] = OpUConvert %uint [[x2u16]]
// CHECK:         [[x3u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3u32:%\d+]] = OpUConvert %uint [[x3u16]]
// CHECK: [[x3u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x3u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x2u32]] [[x3u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address18]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address19:%\d+]] = OpIAdd %uint [[address18]] %uint_1
// CHECK:         [[x4u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4u32:%\d+]] = OpUConvert %uint [[x4u16]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address19]]
// CHECK:                          OpStore [[ptr]] [[x4u32]]

//
// The seventh member of S starts at byte offset 80 (20 words), so:
// for f[0]:
// v should start at byte offset 80 (20 words)
// w should start at byte offset 88 (22 words)
// for f[1]:
// v should start at byte offset 92 (23 words)
// w should start at byte offset 100 (25 words)
//
// CHECK:     [[address20:%\d+]] = OpIAdd %uint [[address0]] %uint_20
// CHECK:             [[f:%\d+]] = OpCompositeExtract %_arr_U_uint_2 [[s0]] 6
// CHECK:            [[u0:%\d+]] = OpCompositeExtract %U [[f]] 0
// CHECK:            [[u1:%\d+]] = OpCompositeExtract %U [[f]] 1
// CHECK:             [[v:%\d+]] = OpCompositeExtract %_arr_half_uint_3 [[u0]] 0
// CHECK:            [[v0:%\d+]] = OpCompositeExtract %half [[v]] 0
// CHECK:            [[v1:%\d+]] = OpCompositeExtract %half [[v]] 1
// CHECK:            [[v2:%\d+]] = OpCompositeExtract %half [[v]] 2
// CHECK:         [[v0u16:%\d+]] = OpBitcast %ushort [[v0]]
// CHECK:         [[v0u32:%\d+]] = OpUConvert %uint [[v0u16]]
// CHECK:         [[v1u16:%\d+]] = OpBitcast %ushort [[v1]]
// CHECK:         [[v1u32:%\d+]] = OpUConvert %uint [[v1u16]]
// CHECK: [[v1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[v1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[v0u32]] [[v1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address20]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK: [[address21:%\d+]] = OpIAdd %uint [[address20]] %uint_1
// CHECK:     [[v2u16:%\d+]] = OpBitcast %ushort [[v2]]
// CHECK:     [[v2u32:%\d+]] = OpUConvert %uint [[v2u16]]
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address21]]
// CHECK:                      OpStore [[ptr]] [[v2u32]]

// CHECK: [[address22:%\d+]] = OpIAdd %uint [[address20]] %uint_2
// CHECK:         [[w:%\d+]] = OpCompositeExtract %uint [[u0]] 1
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address22]]
// CHECK:                      OpStore [[ptr]] [[w]]

// CHECK:     [[address23:%\d+]] = OpIAdd %uint [[address20]] %uint_3
// CHECK:             [[v:%\d+]] = OpCompositeExtract %_arr_half_uint_3 [[u1]] 0
// CHECK:            [[v0:%\d+]] = OpCompositeExtract %half [[v]] 0
// CHECK:            [[v1:%\d+]] = OpCompositeExtract %half [[v]] 1
// CHECK:            [[v2:%\d+]] = OpCompositeExtract %half [[v]] 2
// CHECK:         [[v0u16:%\d+]] = OpBitcast %ushort [[v0]]
// CHECK:         [[v0u32:%\d+]] = OpUConvert %uint [[v0u16]]
// CHECK:         [[v1u16:%\d+]] = OpBitcast %ushort [[v1]]
// CHECK:         [[v1u32:%\d+]] = OpUConvert %uint [[v1u16]]
// CHECK: [[v1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[v1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[v0u32]] [[v1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address23]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address24:%\d+]] = OpIAdd %uint [[address23]] %uint_1
// CHECK:         [[v2u16:%\d+]] = OpBitcast %ushort [[v2]]
// CHECK:         [[v2u32:%\d+]] = OpUConvert %uint [[v2u16]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address24]]
// CHECK:                          OpStore [[ptr]] [[v2u32]]

// CHECK:     [[address25:%\d+]] = OpIAdd %uint [[address23]] %uint_2
// CHECK:             [[w:%\d+]] = OpCompositeExtract %uint [[u1]] 1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address25]]
// CHECK:                          OpStore [[ptr]] [[w]]

//
// The eighth member of S starts at byte offset 104 (26 words)
//
// CHECK: [[address26:%\d+]] = OpIAdd %uint [[address0]] %uint_26
// CHECK:         [[z:%\d+]] = OpCompositeExtract %half [[s0]] 7
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address26]]
// CHECK:      [[zu16:%\d+]] = OpBitcast %ushort [[z]]
// CHECK:      [[zu32:%\d+]] = OpUConvert %uint [[zu16]]
// CHECK:                      OpStore [[ptr]] [[zu32]]

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//
// We have an array of S structures (sArr). The second member (sArr[1]) should
// start at an aligned address. A structure aligment is the maximum alignment
// of its members.
// In this example, sArr[1] should start at byte offset 112 (28 words)
// It should *NOT* start at byte offset 108 (27 words).
//
//
// CHECK: [[address28:%\d+]] = OpIAdd %uint [[address0]] %uint_28
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
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address28]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address29:%\d+]] = OpIAdd %uint [[address28]] %uint_1
// CHECK:         [[a02_16bit:%\d+]] = OpBitcast %ushort [[a02]]
// CHECK:         [[a02_32bit:%\d+]] = OpUConvert %uint [[a02_16bit]]
// CHECK:         [[a10_16bit:%\d+]] = OpBitcast %ushort [[a10]]
// CHECK:         [[a10_32bit:%\d+]] = OpUConvert %uint [[a10_16bit]]
// CHECK: [[a10_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a10_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a02_32bit]] [[a10_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address29]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address30:%\d+]] = OpIAdd %uint [[address29]] %uint_1
// CHECK:         [[a11_16bit:%\d+]] = OpBitcast %ushort [[a11]]
// CHECK:         [[a11_32bit:%\d+]] = OpUConvert %uint [[a11_16bit]]
// CHECK:         [[a12_16bit:%\d+]] = OpBitcast %ushort [[a12]]
// CHECK:         [[a12_32bit:%\d+]] = OpUConvert %uint [[a12_16bit]]
// CHECK: [[a12_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a12_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a11_32bit]] [[a12_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address30]]
// CHECK:                              OpStore [[ptr]] [[word]]

// CHECK:         [[address31:%\d+]] = OpIAdd %uint [[address30]] %uint_1
// CHECK:         [[a20_16bit:%\d+]] = OpBitcast %ushort [[a20]]
// CHECK:         [[a20_32bit:%\d+]] = OpUConvert %uint [[a20_16bit]]
// CHECK:         [[a21_16bit:%\d+]] = OpBitcast %ushort [[a21]]
// CHECK:         [[a21_32bit:%\d+]] = OpUConvert %uint [[a21_16bit]]
// CHECK: [[a21_32bit_shifted:%\d+]] = OpShiftLeftLogical %uint [[a21_32bit]] %uint_16
// CHECK:              [[word:%\d+]] = OpBitwiseOr %uint [[a20_32bit]] [[a21_32bit_shifted]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address31]]
// CHECK:                              OpStore [[ptr]] [[word]]
// CHECK:         [[address32:%\d+]] = OpIAdd %uint [[address31]] %uint_1
// CHECK:         [[a22_16bit:%\d+]] = OpBitcast %ushort [[a22]]
// CHECK:         [[a22_32bit:%\d+]] = OpUConvert %uint [[a22_16bit]]
// CHECK:               [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address32]]
// CHECK:                              OpStore [[ptr]] [[a22_32bit]]

//
// The second member of S starts at byte offset 24 (6 words)
//
// CHECK: [[address34:%\d+]] = OpIAdd %uint [[address28]] %uint_6
//
// CHECK:             [[c:%\d+]] = OpCompositeExtract %double [[s1]] 1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address34]]
// CHECK:         [[c_u64:%\d+]] = OpBitcast %ulong [[c]]
// CHECK:       [[c_word0:%\d+]] = OpUConvert %uint [[c_u64]]
// CHECK: [[c_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[c_u64]] %uint_32
// CHECK:       [[c_word1:%\d+]] = OpUConvert %uint [[c_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[c_word0]]
// CHECK:     [[address35:%\d+]] = OpIAdd %uint [[address34]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address35]]
// CHECK:                          OpStore [[ptr]] [[c_word1]]

//
// The third member of S starts at byte offset 32 (8 words)
//
// CHECK: [[address36:%\d+]] = OpIAdd %uint [[address28]] %uint_8
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
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address36]]
// CHECK:                           OpStore [[ptr]] [[word]]

// CHECK:      [[address37:%\d+]] = OpIAdd %uint [[address36]] %uint_1
// CHECK:         [[x2_u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2_u32:%\d+]] = OpUConvert %uint [[x2_u16]]
// CHECK:         [[x3_u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3_u32:%\d+]] = OpUConvert %uint [[x3_u16:%\d+]]
// CHECK: [[x3_u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x3_u32]] %uint_16
// CHECK:           [[word:%\d+]] = OpBitwiseOr %uint [[x2_u32]] [[x3_u32_shifted]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address37]]
// CHECK:                           OpStore [[ptr]] [[word]]

// CHECK:      [[address38:%\d+]] = OpIAdd %uint [[address37]] %uint_1
// CHECK:         [[x4_u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4_u32:%\d+]] = OpUConvert %uint [[x4_u16]]
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address38]]
// CHECK:                           OpStore [[ptr]] [[x4_u32]]

//
// The fourth member of S starts at byte offset 48 (12 words)
//
// CHECK: [[address40:%\d+]] = OpIAdd %uint [[address28]] %uint_12
//
// CHECK:             [[b:%\d+]] = OpCompositeExtract %double [[s1]] 3
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address40]]
// CHECK:         [[b_u64:%\d+]] = OpBitcast %ulong [[b]]
// CHECK:       [[b_word0:%\d+]] = OpUConvert %uint [[b_u64]]
// CHECK: [[b_u64_shifted:%\d+]] = OpShiftRightLogical %ulong [[b_u64]] %uint_32
// CHECK:       [[b_word1:%\d+]] = OpUConvert %uint [[b_u64_shifted]]
// CHECK:                          OpStore [[ptr]] [[b_word0]]
// CHECK:     [[address41:%\d+]] = OpIAdd %uint [[address40]] %uint_1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address41]]
// CHECK:                          OpStore [[ptr]] [[b_word1]]

//
// The fifth member of S starts at byte offset 56 (14 words)
//
// CHECK: [[address42:%\d+]] = OpIAdd %uint [[address28]] %uint_14
//
// CHECK:     [[d:%\d+]] = OpCompositeExtract %half [[s1]] 4
// CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address42]]
// CHECK: [[d_u16:%\d+]] = OpBitcast %ushort [[d]]
// CHECK: [[d_u32:%\d+]] = OpUConvert %uint [[d_u16]]
// CHECK:                  OpStore [[ptr]] [[d_u32]]

//
// The sixth member of S starts at byte offset 58 (14 words + 16bit offset)
// This is an extraordinary case of alignment. Since the sixth member only
// contains fp16, and the fifth member was also fp16, DX packs them tightly.
// As a result, store must occur at non-aligned offset.
// e[0] takes the following byte offsets: 58, 60, 62, 64, 66.
// e[1] takes the following byte offsets: 68, 70, 72, 74, 76.
// (60-64 = index 15. 64-68 = index 16)
// (68-72 = index 17. 72-76 = index 18)
// (76-78 = first half of index 19)
//
// CHECK:     [[address42:%\d+]] = OpIAdd %uint [[address28]] %uint_14
// CHECK:             [[e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[s1]] 5
// CHECK:            [[e0:%\d+]] = OpCompositeExtract %T [[e]] 0
// CHECK:            [[e1:%\d+]] = OpCompositeExtract %T [[e]] 1
// CHECK:             [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[e0]] 0
// CHECK:            [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:            [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:            [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:            [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:            [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address42]]
// CHECK:         [[x0u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0u32:%\d+]] = OpUConvert %uint [[x0u16]]
// CHECK: [[x0u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x0u32]] %uint_16
// CHECK:  [[existingWord:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:       [[newWord:%\d+]] = OpBitwiseOr %uint [[existingWord]] [[x0u32_shifted]]
// CHECK:                          OpStore [[ptr]] [[newWord]]

// CHECK:     [[address43:%\d+]] = OpIAdd %uint [[address42]] %uint_1
// CHECK:         [[x1u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1u32:%\d+]] = OpUConvert %uint [[x1u16]]
// CHECK:         [[x2u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2u32:%\d+]] = OpUConvert %uint [[x2u16]]
// CHECK: [[x2u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x2u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x1u32]] [[x2u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address43]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address44:%\d+]] = OpIAdd %uint [[address43]] %uint_1
// CHECK:         [[x3u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3u32:%\d+]] = OpUConvert %uint [[x3u16]]
// CHECK:         [[x4u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4u32:%\d+]] = OpUConvert %uint [[x4u16]]
// CHECK: [[x4u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x4u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x3u32]] [[x4u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address44]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address45:%\d+]] = OpIAdd %uint [[address42]] %uint_3
// CHECK:             [[x:%\d+]] = OpCompositeExtract %_arr_half_uint_5 [[e1]] 0
// CHECK:            [[x0:%\d+]] = OpCompositeExtract %half [[x]] 0
// CHECK:            [[x1:%\d+]] = OpCompositeExtract %half [[x]] 1
// CHECK:            [[x2:%\d+]] = OpCompositeExtract %half [[x]] 2
// CHECK:            [[x3:%\d+]] = OpCompositeExtract %half [[x]] 3
// CHECK:            [[x4:%\d+]] = OpCompositeExtract %half [[x]] 4
// CHECK:         [[x0u16:%\d+]] = OpBitcast %ushort [[x0]]
// CHECK:         [[x0u32:%\d+]] = OpUConvert %uint [[x0u16]]
// CHECK:         [[x1u16:%\d+]] = OpBitcast %ushort [[x1]]
// CHECK:         [[x1u32:%\d+]] = OpUConvert %uint [[x1u16]]
// CHECK: [[x1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x0u32]] [[x1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address45]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address46:%\d+]] = OpIAdd %uint [[address45]] %uint_1
// CHECK:         [[x2u16:%\d+]] = OpBitcast %ushort [[x2]]
// CHECK:         [[x2u32:%\d+]] = OpUConvert %uint [[x2u16]]
// CHECK:         [[x3u16:%\d+]] = OpBitcast %ushort [[x3]]
// CHECK:         [[x3u32:%\d+]] = OpUConvert %uint [[x3u16]]
// CHECK: [[x3u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[x3u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[x2u32]] [[x3u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address46]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address47:%\d+]] = OpIAdd %uint [[address46]] %uint_1
// CHECK:         [[x4u16:%\d+]] = OpBitcast %ushort [[x4]]
// CHECK:         [[x4u32:%\d+]] = OpUConvert %uint [[x4u16]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address47]]
// CHECK:                          OpStore [[ptr]] [[x4u32]]

//
// The seventh member of S starts at byte offset 80 (20 words), so:
// for f[0]:
// v should start at byte offset 80 (20 words)
// w should start at byte offset 88 (22 words)
// for f[1]:
// v should start at byte offset 92 (23 words)
// w should start at byte offset 100 (25 words)
//
// CHECK:     [[address48:%\d+]] = OpIAdd %uint [[address28]] %uint_20
// CHECK:             [[f:%\d+]] = OpCompositeExtract %_arr_U_uint_2 [[s1]] 6
// CHECK:            [[u0:%\d+]] = OpCompositeExtract %U [[f]] 0
// CHECK:            [[u1:%\d+]] = OpCompositeExtract %U [[f]] 1
// CHECK:             [[v:%\d+]] = OpCompositeExtract %_arr_half_uint_3 [[u0]] 0
// CHECK:            [[v0:%\d+]] = OpCompositeExtract %half [[v]] 0
// CHECK:            [[v1:%\d+]] = OpCompositeExtract %half [[v]] 1
// CHECK:            [[v2:%\d+]] = OpCompositeExtract %half [[v]] 2
// CHECK:         [[v0u16:%\d+]] = OpBitcast %ushort [[v0]]
// CHECK:         [[v0u32:%\d+]] = OpUConvert %uint [[v0u16]]
// CHECK:         [[v1u16:%\d+]] = OpBitcast %ushort [[v1]]
// CHECK:         [[v1u32:%\d+]] = OpUConvert %uint [[v1u16]]
// CHECK: [[v1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[v1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[v0u32]] [[v1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address48]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK: [[address49:%\d+]] = OpIAdd %uint [[address48]] %uint_1
// CHECK:     [[v2u16:%\d+]] = OpBitcast %ushort [[v2]]
// CHECK:     [[v2u32:%\d+]] = OpUConvert %uint [[v2u16]]
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address49]]
// CHECK:                      OpStore [[ptr]] [[v2u32]]

// CHECK: [[address50:%\d+]] = OpIAdd %uint [[address48]] %uint_2
// CHECK:         [[w:%\d+]] = OpCompositeExtract %uint [[u0]] 1
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address50]]
// CHECK:                      OpStore [[ptr]] [[w]]

// CHECK:     [[address51:%\d+]] = OpIAdd %uint [[address48]] %uint_3
// CHECK:             [[v:%\d+]] = OpCompositeExtract %_arr_half_uint_3 [[u1]] 0
// CHECK:            [[v0:%\d+]] = OpCompositeExtract %half [[v]] 0
// CHECK:            [[v1:%\d+]] = OpCompositeExtract %half [[v]] 1
// CHECK:            [[v2:%\d+]] = OpCompositeExtract %half [[v]] 2
// CHECK:         [[v0u16:%\d+]] = OpBitcast %ushort [[v0]]
// CHECK:         [[v0u32:%\d+]] = OpUConvert %uint [[v0u16]]
// CHECK:         [[v1u16:%\d+]] = OpBitcast %ushort [[v1]]
// CHECK:         [[v1u32:%\d+]] = OpUConvert %uint [[v1u16]]
// CHECK: [[v1u32_shifted:%\d+]] = OpShiftLeftLogical %uint [[v1u32]] %uint_16
// CHECK:          [[word:%\d+]] = OpBitwiseOr %uint [[v0u32]] [[v1u32_shifted]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address51]]
// CHECK:                          OpStore [[ptr]] [[word]]

// CHECK:     [[address52:%\d+]] = OpIAdd %uint [[address51]] %uint_1
// CHECK:         [[v2u16:%\d+]] = OpBitcast %ushort [[v2]]
// CHECK:         [[v2u32:%\d+]] = OpUConvert %uint [[v2u16]]
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address52]]
// CHECK:                          OpStore [[ptr]] [[v2u32]]

// CHECK:     [[address53:%\d+]] = OpIAdd %uint [[address51]] %uint_2
// CHECK:             [[w:%\d+]] = OpCompositeExtract %uint [[u1]] 1
// CHECK:           [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address53]]
// CHECK:                          OpStore [[ptr]] [[w]]

//
// The eighth member of S starts at byte offset 104 (26 words)
//
// CHECK: [[address54:%\d+]] = OpIAdd %uint [[address28]] %uint_26
// CHECK:         [[z:%\d+]] = OpCompositeExtract %half [[s1]] 7
// CHECK:       [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address54]]
// CHECK:      [[zu16:%\d+]] = OpBitcast %ushort [[z]]
// CHECK:      [[zu32:%\d+]] = OpUConvert %uint [[zu16]]
// CHECK:                      OpStore [[ptr]] [[zu32]]
