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

// CHECK:      [[tidx_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %tid %int_0
// CHECK:          [[tidx:%\d+]] = OpLoad %uint [[tidx_ptr]]
// CHECK:      [[address0:%\d+]] = OpShiftRightLogical %uint [[tidx]] %uint_2
// CHECK:          [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address0]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK:      [[word0u16:%\d+]] = OpUConvert %ushort [[word0]]
// CHECK:             [[a:%\d+]] = OpBitcast %half [[word0u16]]
// CHECK:          [[ptr0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address0]]
// CHECK:         [[word0:%\d+]] = OpLoad %uint [[ptr0]]
// CHECK:    [[word0upper:%\d+]] = OpShiftRightLogical %uint [[word0]] %uint_16
// CHECK: [[word0upperu16:%\d+]] = OpUConvert %ushort [[word0upper]]
// CHECK:           [[x_0:%\d+]] = OpBitcast %half [[word0upperu16]]
// CHECK:      [[address1:%\d+]] = OpIAdd %uint [[address0]] %uint_1
// CHECK:          [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK:      [[word1u16:%\d+]] = OpUConvert %ushort [[word1]]
// CHECK:           [[x_1:%\d+]] = OpBitcast %half [[word1u16]]
// CHECK:             [[x:%\d+]] = OpCompositeConstruct %_arr_half_uint_2 [[x_0]] [[x_1]]
// CHECK:      [[address1:%\d+]] = OpIAdd %uint [[address0]] %uint_1
// CHECK:           [[e_0:%\d+]] = OpCompositeConstruct %T [[x]]
// CHECK:          [[ptr1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address1]]
// CHECK:         [[word1:%\d+]] = OpLoad %uint [[ptr1]]
// CHECK:    [[word1upper:%\d+]] = OpShiftRightLogical %uint [[word1]] %uint_16
// CHECK: [[word1upperu16:%\d+]] = OpUConvert %ushort [[word1upper]]
// CHECK:           [[x_0:%\d+]] = OpBitcast %half [[word1upperu16]]
// CHECK:      [[address2:%\d+]] = OpIAdd %uint [[address1]] %uint_1
// CHECK:          [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK:      [[word2u16:%\d+]] = OpUConvert %ushort [[word2]]
// CHECK:           [[x_1:%\d+]] = OpBitcast %half [[word2u16]]
// CHECK:             [[x:%\d+]] = OpCompositeConstruct %_arr_half_uint_2 [[x_0]] [[x_1]]
// CHECK:           [[e_1:%\d+]] = OpCompositeConstruct %T [[x]]
// CHECK:             [[e:%\d+]] = OpCompositeConstruct %_arr_T_uint_2 [[e_0]] [[e_1]]
// CHECK:      [[address2:%\d+]] = OpIAdd %uint [[address0]] %uint_2
// CHECK:           [[s_0:%\d+]] = OpCompositeConstruct %S [[a]] [[e]]
//
// Now start with the second 'S' object
//
// CHECK:          [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address2]]
// CHECK:         [[word2:%\d+]] = OpLoad %uint [[ptr2]]
// CHECK:  [[word2upper16:%\d+]] = OpShiftRightLogical %uint [[word2]] %uint_16
// CHECK: [[word2upperu16:%\d+]] = OpUConvert %ushort [[word2upper16]]
// CHECK:             [[a:%\d+]] = OpBitcast %half [[word2upperu16]]
// CHECK:      [[address3:%\d+]] = OpIAdd %uint [[address2]] %uint_1
// CHECK:      [[address3:%\d+]] = OpIAdd %uint [[address2]] %uint_1
// CHECK:               {{%\d+}} = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address3]]
// CHECK:               {{%\d+}} = OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address3]]
// CHECK:                          OpCompositeConstruct %_arr_half_uint_2
// CHECK:      [[address4:%\d+]] = OpIAdd %uint [[address3]] %uint_1
// CHECK:                          OpCompositeConstruct %T
// CHECK:                          OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address4]]
// CHECK:                          OpAccessChain %_ptr_Uniform_uint %buf %uint_0 [[address4]]
// CHECK:                          OpCompositeConstruct %_arr_half_uint_2
// CHECK:                          OpCompositeConstruct %T
// CHECK:                          OpCompositeConstruct %_arr_T_uint_2
// CHECK:                          OpCompositeConstruct %S
// CHECK:                          OpCompositeConstruct %_arr_S_uint_2
// CHECK:                          OpStore %sArr {{%\d+}}
