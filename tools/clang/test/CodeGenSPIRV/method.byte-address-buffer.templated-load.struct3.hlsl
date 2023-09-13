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

// CHECK:      [[tidx_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %tid %int_0
// CHECK:  [[base_address:%\d+]] = OpLoad %uint [[tidx_ptr]]
// CHECK:       [[a_index:%\d+]] = OpShiftRightLogical %uint [[base_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[base_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[a_index]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK:       [[shifted:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOffset]]
// CHECK:      [[word0u16:%\d+]] = OpUConvert %ushort [[shifted]]
// CHECK:             [[a:%\d+]] = OpBitcast %half [[word0u16]]
// CHECK:    [[e0_address:%\d+]] = OpIAdd %uint [[base_address]] %uint_2
// CHECK:    [[e0_address:%\d+]] = OpIAdd %uint [[base_address]] %uint_2
// CHECK:      [[e0_index:%\d+]] = OpShiftRightLogical %uint [[e0_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[e0_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[e0_index]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK:    [[word0upper:%\d+]] = OpShiftRightLogical %uint [[word0]] [[bitOffset]]
// CHECK: [[word0upperu16:%\d+]] = OpUConvert %ushort [[word0upper]]
// CHECK:           [[x_0:%\d+]] = OpBitcast %half [[word0upperu16]]
// CHECK:    [[x1_address:%\d+]] = OpIAdd %uint [[e0_address]] %uint_2
// CHECK:      [[x1_index:%\d+]] = OpShiftRightLogical %uint [[x1_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[x1_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[x1_index]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK:       [[shifted:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOffset]]
// CHECK:      [[word1u16:%\d+]] = OpUConvert %ushort [[shifted]]
// CHECK:           [[x_1:%\d+]] = OpBitcast %half [[word1u16]]
// CHECK:             [[x:%\d+]] = OpCompositeConstruct %_arr_half_uint_2 [[x_0]] [[x_1]]
// CHECK:    [[e1_address:%\d+]] = OpIAdd %uint [[e0_address]] %uint_4
// CHECK:           [[e_0:%\d+]] = OpCompositeConstruct %T [[x]]
// CHECK:      [[e1_index:%\d+]] = OpShiftRightLogical %uint [[e1_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[e1_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[e1_index]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK:    [[word1upper:%\d+]] = OpShiftRightLogical %uint [[word1]] [[bitOffset]]
// CHECK: [[word1upperu16:%\d+]] = OpUConvert %ushort [[word1upper]]
// CHECK:           [[x_0:%\d+]] = OpBitcast %half [[word1upperu16]]
// CHECK:    [[x1_address:%\d+]] = OpIAdd %uint [[e1_address]] %uint_2
// CHECK:      [[x1_index:%\d+]] = OpShiftRightLogical %uint [[x1_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[x1_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[x1_index]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK:       [[shifted:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOffset]]
// CHECK:      [[word2u16:%\d+]] = OpUConvert %ushort [[shifted]]
// CHECK:           [[x_1:%\d+]] = OpBitcast %half [[word2u16]]
// CHECK:             [[x:%\d+]] = OpCompositeConstruct %_arr_half_uint_2 [[x_0]] [[x_1]]
// CHECK:           [[e_1:%\d+]] = OpCompositeConstruct %T [[x]]
// CHECK:             [[e:%\d+]] = OpCompositeConstruct %_arr_T_uint_2 [[e_0]] [[e_1]]
// CHECK:    [[s1_address:%\d+]] = OpIAdd %uint [[base_address]] %uint_10
// CHECK:           [[s_0:%\d+]] = OpCompositeConstruct %S [[a]] [[e]]
//
// Now start with the second 'S' object
//
// CHECK:      [[s1_index:%\d+]] = OpShiftRightLogical %uint [[s1_address]] %uint_2
// CHECK:    [[byteOffset:%\d+]] = OpUMod %uint [[s1_address]] %uint_4
// CHECK:     [[bitOffset:%\d+]] = OpShiftLeftLogical %uint [[byteOffset]] %uint_3
// CHECK:          [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[s1_index]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK:  [[word2upper16:%\d+]] = OpShiftRightLogical %uint [[word2]] [[bitOffset]]
// CHECK: [[word2upperu16:%\d+]] = OpUConvert %ushort [[word2upper16]]
// CHECK:             [[a:%\d+]] = OpBitcast %half [[word2upperu16]]
// CHECK:    [[e0_address:%\d+]] = OpIAdd %uint [[s1_address]] %uint_2
// CHECK:    [[e0_address:%\d+]] = OpIAdd %uint [[s1_address]] %uint_2
// CHECK:      [[e0_index:%\d+]] = OpShiftRightLogical %uint [[e0_address]] %uint_2
// CHECK:               {{%\d+}} = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[e0_index]]
// CHECK:    [[x1_address:%\d+]] = OpIAdd %uint [[e0_address]] %uint_2
// CHECK:      [[x1_index:%\d+]] = OpShiftRightLogical %uint [[x1_address]] %uint_2
// CHECK:               {{%\d+}} = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[x1_index]]
// CHECK:                          OpCompositeConstruct %_arr_half_uint_2
// CHECK:    [[e1_address:%\d+]] = OpIAdd %uint [[e0_address]] %uint_4
// CHECK:                          OpCompositeConstruct %T
// CHECK:      [[e1_index:%\d+]] = OpShiftRightLogical %uint [[e1_address]] %uint_
// CHECK:                          OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[e1_index]]
// CHECK:    [[x1_address:%\d+]] = OpIAdd %uint [[e1_address]] %uint_2
// CHECK:      [[x1_index:%\d+]] = OpShiftRightLogical %uint [[x1_address]] %uint_2
// CHECK:                          OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[x1_index]]
// CHECK:                          OpCompositeConstruct %_arr_half_uint_2
// CHECK:                          OpCompositeConstruct %T
// CHECK:                          OpCompositeConstruct %_arr_T_uint_2
// CHECK:                          OpCompositeConstruct %S
// CHECK:                          OpCompositeConstruct %_arr_S_uint_2
// CHECK:                          OpStore %sArr {{%\d+}}
