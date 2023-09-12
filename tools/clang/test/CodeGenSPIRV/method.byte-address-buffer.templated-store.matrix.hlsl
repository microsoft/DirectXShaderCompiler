// RUN: %dxc -T cs_6_2 -E main
//
// In this test, check that matrix order is preserved on a templated store.

ByteAddressBuffer buf;
RWByteAddressBuffer buf2;

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadId)
{
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
// CHECK:   [[row0:%\d+]] = OpCompositeConstruct %v2int [[val0]] [[val2]]
// CHECK:   [[row1:%\d+]] = OpCompositeConstruct %v2int [[val1]] [[val3]]
// CHECK:   [[mat0:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_2 [[row0]] [[row1]]
// CHECK:                   OpStore [[temp:%\w+]] [[mat0]]
// CHECK:   [[mat1:%\d+]] = OpLoad %_arr_v2int_uint_2 [[temp]]
// CHECK:  [[elem0:%\d+]] = OpCompositeExtract %int [[mat1]] 0 0
// CHECK:  [[elem1:%\d+]] = OpCompositeExtract %int [[mat1]] 1 0
// CHECK:  [[elem2:%\d+]] = OpCompositeExtract %int [[mat1]] 0 1
// CHECK:  [[elem3:%\d+]] = OpCompositeExtract %int [[mat1]] 1 1
// CHECK:   [[idx0:%\d+]] = OpShiftRightLogical %uint [[addr0:%\d+]] %uint_2
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[idx0]]
// CHECK:    [[val:%\d+]] = OpBitcast %uint [[elem0]]
// CHECK:                   OpStore [[ptr]] [[val]]
// CHECK:   [[idx1:%\d+]] = OpIAdd %uint [[idx0]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[idx1]]
// CHECK:    [[val:%\d+]] = OpBitcast %uint [[elem1]]
// CHECK:                   OpStore [[ptr]] [[val]]
// CHECK:   [[idx2:%\d+]] = OpIAdd %uint [[idx1]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[idx2]]
// CHECK:    [[val:%\d+]] = OpBitcast %uint [[elem2]]
// CHECK:                   OpStore [[ptr]] [[val]]
// CHECK:   [[idx3:%\d+]] = OpIAdd %uint [[idx2]] %uint_1
// CHECK:    [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %buf2 %uint_0 [[idx3]]
// CHECK:    [[val:%\d+]] = OpBitcast %uint [[elem3]]
// CHECK:                   OpStore [[ptr]] [[val]]

  int2x2 i = buf.Load<int2x2>(tid.x);
  buf2.Store<int2x2>(tid.x, i);
}
