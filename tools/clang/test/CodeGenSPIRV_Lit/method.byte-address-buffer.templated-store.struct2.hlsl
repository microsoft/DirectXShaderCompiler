// RUN: %dxc -T cs_6_2 -E main -enable-16bit-types -fvk-use-dx-layout

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

// a

// CHECK: OpStore %sArr
// CHECK:     [[tidx:%\d+]] = OpAccessChain %_ptr_Function_uint %tid %int_0
// CHECK:  [[s0_addr:%\d+]] = OpLoad %uint [[tidx]]
// CHECK:     [[sArr:%\d+]] = OpLoad %_arr_S_uint_2 %sArr
// CHECK:    [[sArr0:%\d+]] = OpCompositeExtract %S [[sArr]] 0
// CHECK:    [[sArr1:%\d+]] = OpCompositeExtract %S [[sArr]] 1
// CHECK:     [[s0_a:%\d+]] = OpCompositeExtract %half [[sArr0]] 0
// CHECK: [[s0_a_ind:%\d+]] = OpShiftRightLogical %uint [[s0_addr]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[s0_addr]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s0_a_ind]]
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK:  [[shifted:%\d+]] = OpShiftLeftLogical %uint {{%\d+}} [[bitOff]]
// CHECK:  [[maskOff:%\d+]] = OpISub %uint %uint_16 [[bitOff]]
// CHECK:     [[mask:%\d+]] = OpShiftLeftLogical %uint %uint_65535 [[maskOff]]
// CHECK: [[oldWord0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK: [[maskWord:%\d+]] = OpBitwiseAnd %uint [[oldWord0]] [[mask]]
// CHECK: [[newWord0:%\d+]] = OpBitwiseOr %uint [[maskWord]] [[shifted]]
// CHECK: OpStore [[ptr0]] [[newWord0]]

// e[0].x[0]

// CHECK:                     OpIAdd %uint [[s0_addr]] %uint_2
// CHECK:    [[eAddr:%\d+]] = OpIAdd %uint [[s0_addr]] %uint_2
// CHECK:     [[s0_e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[sArr0]] 1
// CHECK:    [[s0_e0:%\d+]] = OpCompositeExtract %T [[s0_e]] 0
// CHECK:    [[s0_e1:%\d+]] = OpCompositeExtract %T [[s0_e]] 1
// CHECK:  [[s0_e0_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s0_e0]] 0
// CHECK: [[s0_e0_x0:%\d+]] = OpCompositeExtract %half [[s0_e0_x]] 0
// CHECK: [[s0_e0_x1:%\d+]] = OpCompositeExtract %half [[s0_e0_x]] 1
// CHECK: [[s0_e_ind:%\d+]] = OpShiftRightLogical %uint [[eAddr]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[eAddr]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s0_e_ind]]
// CHECK: OpBitcast %ushort [[s0_e0_x0]]
// CHECK: OpUConvert %uint
// CHECK:  [[shifted:%\d+]] = OpShiftLeftLogical %uint {{%\d+}} [[bitOff]]
// CHECK:  [[maskOff:%\d+]] = OpISub %uint %uint_16 [[bitOff]]
// CHECK:     [[mask:%\d+]] = OpShiftLeftLogical %uint %uint_65535 [[maskOff]]
// CHECK: [[oldWord0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK: [[maskWord:%\d+]] = OpBitwiseAnd %uint [[oldWord0]] [[mask]]
// CHECK: [[newWord0:%\d+]] = OpBitwiseOr %uint [[maskWord]] [[shifted]]
// CHECK:                     OpStore [[ptr0]] [[newWord0]]

// e[0].x[1]

// CHECK:[[s0_e0_x1_add:%\d+]] = OpIAdd %uint [[eAddr]] %uint_2
// CHECK:[[s0_e0_x1_ind:%\d+]] = OpShiftRightLogical %uint [[s0_e0_x1_add]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[s0_e0_x1_add]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s0_e0_x1_ind]]
// CHECK: OpBitcast %ushort [[s0_e0_x1]]
// CHECK: OpUConvert %uint
// CHECK:  [[shifted:%\d+]] = OpShiftLeftLogical %uint {{%\d+}} [[bitOff]]
// CHECK:  [[maskOff:%\d+]] = OpISub %uint %uint_16 [[bitOff]]
// CHECK:     [[mask:%\d+]] = OpShiftLeftLogical %uint %uint_65535 [[maskOff]]
// CHECK: [[oldWord1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK: [[maskWord:%\d+]] = OpBitwiseAnd %uint [[oldWord1]] [[mask]]
// CHECK: [[newWord1:%\d+]] = OpBitwiseOr %uint [[maskWord]] [[shifted]]
// CHECK:                     OpStore [[ptr1]] [[newWord1]]

// e[1].x[0]

// CHECK:   [[e1Addr:%\d+]] = OpIAdd %uint [[eAddr]] %uint_4
// CHECK:  [[s0_e1_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s0_e1]] 0
// CHECK: [[s0_e1_x0:%\d+]] = OpCompositeExtract %half [[s0_e1_x]] 0
// CHECK: [[s0_e1_x1:%\d+]] = OpCompositeExtract %half [[s0_e1_x]] 1
// CHECK:[[s0_e1_x0_ind:%\d+]] = OpShiftRightLogical %uint [[e1Addr]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[e1Addr]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s0_e1_x0_ind]]
// CHECK: OpBitcast %ushort [[s0_e1_x0]]
// CHECK: OpUConvert %uint
// CHECK:  [[shifted:%\d+]] = OpShiftLeftLogical %uint {{%\d+}} [[bitOff]]
// CHECK:  [[maskOff:%\d+]] = OpISub %uint %uint_16 [[bitOff]]
// CHECK:     [[mask:%\d+]] = OpShiftLeftLogical %uint %uint_65535 [[maskOff]]
// CHECK: [[oldWord1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK: [[maskWord:%\d+]] = OpBitwiseAnd %uint [[oldWord1]] [[mask]]
// CHECK: [[newWord1:%\d+]] = OpBitwiseOr %uint [[maskWord]] [[shifted]]
// CHECK:                     OpStore [[ptr1]] [[newWord1]]

// e[1].x[1]

// CHECK:[[s0_e1_x1_add:%\d+]] = OpIAdd %uint [[e1Addr]] %uint_2
// CHECK:[[s0_e1_x1_ind:%\d+]] = OpShiftRightLogical %uint [[s0_e1_x1_add]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[s0_e1_x1_add]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s0_e1_x1_ind]]
// CHECK: OpBitcast %ushort [[s0_e1_x1]]
// CHECK: OpUConvert %uint
// CHECK:                     OpStore [[ptr2]] {{%\d+}}

// a

// CHECK:  [[s1_addr:%\d+]] = OpIAdd %uint [[s0_addr]] %uint_10
// CHECK:     [[s1_a:%\d+]] = OpCompositeExtract %half [[sArr1]] 0
// CHECK: [[s1_a_ind:%\d+]] = OpShiftRightLogical %uint [[s1_addr]] %uint_2
// CHECK:  [[byteOff:%\d+]] = OpUMod %uint [[s1_addr]] %uint_4
// CHECK:   [[bitOff:%\d+]] = OpShiftLeftLogical %uint [[byteOff]] %uint_3
// CHECK:     [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[s1_a_ind]]
// CHECK: OpBitcast %ushort [[s1_a]]
// CHECK: OpUConvert %uint
// CHECK:  [[shifted:%\d+]] = OpShiftLeftLogical %uint {{%\d+}} [[bitOff]]
// CHECK:  [[maskOff:%\d+]] = OpISub %uint %uint_16 [[bitOff]]
// CHECK:     [[mask:%\d+]] = OpShiftLeftLogical %uint %uint_65535 [[maskOff]]
// CHECK: [[oldWord2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK: [[maskWord:%\d+]] = OpBitwiseAnd %uint [[oldWord2]] [[mask]]
// CHECK: [[newWord2:%\d+]] = OpBitwiseOr %uint [[maskWord]] [[shifted]]
// CHECK:                     OpStore [[ptr2]] [[newWord2]]

// e[0].x[0]
// e[0].x[1]

// CHECK:    [[eAddr:%\d+]] = OpIAdd %uint [[s1_addr]] %uint_2
// CHECK:    [[eAddr:%\d+]] = OpIAdd %uint [[s1_addr]] %uint_2
// CHECK:     [[s1_e:%\d+]] = OpCompositeExtract %_arr_T_uint_2 [[sArr1]] 1
// CHECK:    [[s1_e0:%\d+]] = OpCompositeExtract %T [[s1_e]] 0
// CHECK:    [[s1_e1:%\d+]] = OpCompositeExtract %T [[s1_e]] 1
// CHECK:  [[s1_e0_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s1_e0]] 0
// CHECK: [[s1_e0_x0:%\d+]] = OpCompositeExtract %half [[s1_e0_x]] 0
// CHECK: [[s1_e0_x1:%\d+]] = OpCompositeExtract %half [[s1_e0_x]] 1
// CHECK: OpBitcast %ushort [[s1_e0_x0]]
// CHECK: OpUConvert %uint
// CHECK:[[index:%\d+]] = OpShiftRightLogical %uint {{%\d+}}
// CHECK: [[ptr3:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[index]]
// CHECK: OpBitcast %ushort [[s1_e0_x1]]
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} {{%\d+}}
// CHECK: OpBitwiseOr %uint
// CHECK: OpStore [[ptr3]] {{%\d+}}

// e[1].x[0]
// e[1].x[1]

// CHECK:   [[e1Addr:%\d+]] = OpIAdd %uint [[eAddr]] %uint_4
// CHECK:  [[s1_e1_x:%\d+]] = OpCompositeExtract %_arr_half_uint_2 [[s1_e1]] 0
// CHECK: [[s1_e1_x0:%\d+]] = OpCompositeExtract %half [[s1_e1_x]] 0
// CHECK: [[s1_e1_x1:%\d+]] = OpCompositeExtract %half [[s1_e1_x]] 1
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK:[[index:%\d+]] = OpShiftRightLogical %uint {{%\d+}}
// CHECK: [[ptr4:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[index]]
// CHECK: OpBitcast %ushort
// CHECK: OpUConvert %uint
// CHECK: OpShiftLeftLogical %uint {{%\d+}} {{%\d+}}
// CHECK: OpBitwiseOr %uint
// CHECK: OpStore [[ptr4]] {{%\d+}}
