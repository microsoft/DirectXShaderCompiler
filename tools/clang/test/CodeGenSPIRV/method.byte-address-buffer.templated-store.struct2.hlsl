// Run: %dxc -T cs_6_2 -E main -enable-16bit-types -fvk-use-dx-layout

ByteAddressBuffer buf;
RWByteAddressBuffer buf2;

struct T {
  float16_t x[2];
};

struct S {
  float16_t a;
  T e[2];
};

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId) {
  S sArr[2] = buf.Load<S[2]>(tid.x);
  buf2.Store<S[2]>(tid.x, sArr);
}

// Note: the DX layout tightly packs all members of S and its sub-structures.
// It stores elements at the following byte offsets:
// 0, 2, 4, 6, 8, 10, 12, 14, 16, 18
//
//                              |-----------------------|
// address 0:                   |     a     | e[0].x[0] |
//                              |-----------------------|
// address 1 (byte offset 4):   | e[0].x[1] | e[1].x[0] |
//                              |-----------------------|
// address 2 (byte offset 8):   | e[1].x[1] |     a     |
//                              |-----------------------|
// address 3 (byte offset 12)   | e[0].x[0] | e[0].x[1] |
//                              |-----------------------|
// address 4 (byte offset 16)   | e[1].x[0] | e[1].x[1] |
//                              |-----------------------|
//

// CHECK: OpStore %sArr
// CHECK: OpAccessChain %_ptr_Function_uint %tid %int_0
// CHECK: [[address0:%\d+]] = OpShiftRightLogical %uint {{%\d+}} %uint_2
// CHECK:     [[sArr:%\d+]] = OpLoad %_arr_S_uint_2 %sArr
// CHECK:    [[sArr0:%\d+]] = OpCompositeExtract %S [[sArr]] 0
// CHECK:    [[sArr1:%\d+]] = OpCompositeExtract %S [[sArr]] 1
// CHECK:     [[s0_a:%\d+]] = OpCompositeExtract %half [[sArr]] 0
// CHECK:     [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address0]]
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK: OpStore [[ptr0]]
// CHECK:     [[s0_e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[sArr0]] 1
// CHECK:    [[s0_e0:%\d+]] = OpCompositeExtract %T [[s0_e]] 0
// CHECK:    [[s0_e1:%\d+]] = OpCompositeExtract %T [[s0_e]] 1
// CHECK:  [[s0_e0_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s0_e0]] 0
// CHECK: [[s0_e0_x0:%\d+]] = OpCompositeExtract %half [[s0_e0_x]] 0
// CHECK: [[s0_e0_x1:%\d+]] = OpCompositeExtract %half [[s0_e0_x]] 1
// CHECK:     [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address0]]
// CHECK: OpBitcast %ushort [[s0_e0_x0]]
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint
// CHECK: [[oldWord0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK: [[newWord0:%\d+]] = OpBitwiseOr %uint [[oldWord0]] {{%\d+}}
// CHECK:                     OpStore [[ptr0]] [[newWord0]]
// CHECK: [[address1:%\d+]] = OpIAdd %uint [[address0]] %uint_1
// CHECK: OpBitcast %ushort [[s0_e0_x1]]
// CHECK: OpUConvert %uint
// CHECK:     [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address1]]
// CHECK:                     OpStore [[ptr1]] {{%\d+}}
// CHECK: [[address1:%\d+]] = OpIAdd %uint [[address0]] %uint_1
// CHECK:  [[s0_e1_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s0_e1]] 0
// CHECK: [[s0_e1_x0:%\d+]] = OpCompositeExtract %half [[s0_e1_x]] 0
// CHECK: [[s0_e1_x1:%\d+]] = OpCompositeExtract %half [[s0_e1_x]] 1
// CHECK:     [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address1]]
// CHECK: OpBitcast %ushort [[s0_e1_x0]]
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} %uint_16
// CHECK: [[oldWord1:%\d+]] = OpLoad %uint [[address1]]
// CHECK: [[newWord1:%\d+]] = OpBitwiseOr %uint [[oldWord1]] {{%\d+}}
// CHECK:                     OpStore [[ptr1]] [[newWord1]]

// CHECK: [[address2:%\d+]] = OpIAdd %uint [[address1]] %uint_1
// CHECK: OpBitcast %ushort [[s0_e1_x1]]
// CHECK: OpUConvert %uint
// CHECK:     [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address2]]
// CHECK:                     OpStore [[ptr2]] {{%\d+}}
// CHECK: [[address2:%\d+]] = OpIAdd %uint [[address0]] %uint_2
// CHECK:     [[s1_a:%\d+]] = OpCompositeExtract %half [[sArr1]] 0
// CHECK:     [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address2]]
// CHECK: OpBitcast %ushort [[s1_a]]
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} %uint_16
// CHECK: [[oldWord2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK: [[newWord2:%\d+]] = OpBitwiseOr %uint [[oldWord2]] {{%\d+}}
// CHECK:                     OpStore [[ptr2]] [[newWord2]]

// CHECK: [[address3:%\d+]] = OpIAdd %uint [[address2]] %uint_1
// CHECK: [[address3:%\d+]] = OpIAdd %uint [[address2]] %uint_1
// CHECK:     [[s1_e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[sArr1]] 1
// CHECK:    [[s1_e0:%\d+]] = OpCompositeExtract %T [[s1_e]] 0
// CHECK:    [[s1_e1:%\d+]] = OpCompositeExtract %T [[s1_e]] 1
// CHECK:  [[s1_e0_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s1_e0]] 0
// CHECK: [[s1_e0_x0:%\d+]] = OpCompositeExtract %half [[s1_e0_x]] 0
// CHECK: [[s1_e0_x1:%\d+]] = OpCompositeExtract %half [[s1_e0_x]] 1
// CHECK: OpBitcast %ushort [[s1_e0_x0]]
// CHECK: OpUConvert %uint
// CHECK: OpBitcast %ushort [[s1_e0_x1]]
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} %uint_16
// CHECK: OpBitwiseOr %uint
// CHECK: [[ptr3:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address3]]
// CHECK: OpStore [[ptr3]] {{%\d+}}

// CHECK: [[address4:%\d+]] = OpIAdd %uint [[address3]] %uint_1
// CHECK: [[address4:%\d+]] = OpIAdd %uint [[address3]] %uint_1
// CHECK:  [[s1_e1_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s1_e1]] 0
// CHECK: [[s1_e1_x0:%\d+]] = OpCompositeExtract %half [[s1_e1_x]] 0
// CHECK: [[s1_e1_x1:%\d+]] = OpCompositeExtract %half [[s1_e1_x]] 1
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} %uint_16
// CHECK: OpBitwiseOr %uint
// CHECK: [[ptr4:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[address4]]
// CHECK: OpStore [[ptr4]] {{%\d+}}
