// RUN: %dxc -T cs_6_0 -E main

// Processing FlatConversion when source and destination
// are both structures with identical members.

struct FirstStruct {
  float3 anArray[4];
  float2x3 mats[1];
  int2 ints[3];
};

struct SecondStruct {
  float3 anArray[4];
  float2x3 mats[1];
  int2 ints[3];
};

RWStructuredBuffer<FirstStruct> rwBuf : register(u0);
[ numthreads ( 16 , 16 , 1 ) ]
void main() {
  SecondStruct values;
  FirstStruct v;

// Yes, this is a FlatConversion!
// CHECK:      [[v0ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_v3float_uint_4_0 %values %int_0
// CHECK-NEXT:    [[v0:%\d+]] = OpLoad %_arr_v3float_uint_4_0 [[v0ptr]]
// CHECK-NEXT: [[v1ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_mat2v3float_uint_1_0 %values %int_1
// CHECK-NEXT:    [[v1:%\d+]] = OpLoad %_arr_mat2v3float_uint_1_0 [[v1ptr]]
// CHECK-NEXT: [[v2ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_v2int_uint_3_0 %values %int_2
// CHECK-NEXT:    [[v2:%\d+]] = OpLoad %_arr_v2int_uint_3_0 [[v2ptr]]
// CHECK-NEXT:     [[v:%\d+]] = OpCompositeConstruct %FirstStruct_0 [[v0]] [[v1]] [[v2]]
// CHECK-NEXT:                  OpStore %v [[v]]
  v = values;

// CHECK:          [[v0ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_v3float_uint_4_0 %values %int_0
// CHECK-NEXT:        [[v0:%\d+]] = OpLoad %_arr_v3float_uint_4_0 [[v0ptr]]
// CHECK-NEXT:     [[v1ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_mat2v3float_uint_1_0 %values %int_1
// CHECK-NEXT:        [[v1:%\d+]] = OpLoad %_arr_mat2v3float_uint_1_0 [[v1ptr]]
// CHECK-NEXT:     [[v2ptr:%\d+]] = OpAccessChain %_ptr_Function__arr_v2int_uint_3_0 %values %int_2
// CHECK-NEXT:        [[v2:%\d+]] = OpLoad %_arr_v2int_uint_3_0 [[v2ptr]]
// CHECK-NEXT:    [[values:%\d+]] = OpCompositeConstruct %FirstStruct_0 [[v0]] [[v1]] [[v2]]
// CHECK-NEXT: [[rwBuf_ptr:%\d+]] = OpAccessChain %_ptr_Uniform_FirstStruct %rwBuf %int_0 %uint_0
// CHECK-NEXT:   [[anArray:%\d+]] = OpCompositeExtract %_arr_v3float_uint_4_0 [[values]] 0
// CHECK-NEXT:  [[anArray1:%\d+]] = OpCompositeExtract %v3float [[anArray]] 0
// CHECK-NEXT:  [[anArray2:%\d+]] = OpCompositeExtract %v3float [[anArray]] 1
// CHECK-NEXT:  [[anArray3:%\d+]] = OpCompositeExtract %v3float [[anArray]] 2
// CHECK-NEXT:  [[anArray4:%\d+]] = OpCompositeExtract %v3float [[anArray]] 3
// CHECK-NEXT:      [[res1:%\d+]] = OpCompositeConstruct %_arr_v3float_uint_4 [[anArray1]] [[anArray2]] [[anArray3]] [[anArray4]]
// CHECK-NEXT:      [[mats:%\d+]] = OpCompositeExtract %_arr_mat2v3float_uint_1_0 [[values]] 1
// CHECK-NEXT:       [[mat:%\d+]] = OpCompositeExtract %mat2v3float [[mats]] 0
// CHECK-NEXT:      [[res2:%\d+]] = OpCompositeConstruct %_arr_mat2v3float_uint_1 [[mat]]
// CHECK-NEXT:      [[ints:%\d+]] = OpCompositeExtract %_arr_v2int_uint_3_0 [[values]] 2
// CHECK-NEXT:     [[ints1:%\d+]] = OpCompositeExtract %v2int [[ints]] 0
// CHECK-NEXT:     [[ints2:%\d+]] = OpCompositeExtract %v2int [[ints]] 1
// CHECK-NEXT:     [[ints3:%\d+]] = OpCompositeExtract %v2int [[ints]] 2
// CHECK-NEXT:      [[res3:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_3 [[ints1]] [[ints2]] [[ints3]]
// CHECK-NEXT:    [[result:%\d+]] = OpCompositeConstruct %FirstStruct [[res1]] [[res2]] [[res3]]
// CHECK-NEXT:                      OpStore [[rwBuf_ptr]] [[result]]
  rwBuf[0] = values;
}
