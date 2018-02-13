// Run: %dxc -T ps_6_0 -E main

struct VSOutput {
  float4   sv_pos     : SV_POSITION;
  uint3    normal     : NORMAL;
  int2     tex_coord  : TEXCOORD;
  bool     mybool[2]  : MYBOOL;
  int      arr[5]     : MYARRAY;
  float2x3 mat2x3     : MYMATRIX;
  int2x3   intmat     : MYINTMATRIX;
  bool2x3  boolmat    : MYBOOLMATRIX;
};


// CHECK: [[nullVSOutput:%\d+]] = OpConstantNull %VSOutput


void main() {
  int x = 3;

// CHECK: OpStore %output1 [[nullVSOutput]]
  VSOutput output1 = (VSOutput)0;
// CHECK: OpStore %output2 [[nullVSOutput]]
  VSOutput output2 = (VSOutput)0.0;
// CHECK: OpStore %output3 [[nullVSOutput]]
  VSOutput output3 = (VSOutput)false;

// CHECK:                [[f1:%\d+]] = OpConvertSToF %float %int_1
// CHECK-NEXT:         [[v4f1:%\d+]] = OpCompositeConstruct %v4float [[f1]] [[f1]] [[f1]] [[f1]]
// CHECK-NEXT:           [[u1:%\d+]] = OpBitcast %uint %int_1
// CHECK-NEXT:         [[v3u1:%\d+]] = OpCompositeConstruct %v3uint [[u1]] [[u1]] [[u1]]
// CHECK-NEXT:         [[v2i1:%\d+]] = OpCompositeConstruct %v2int %int_1 %int_1
// CHECK-NEXT:        [[bool1:%\d+]] = OpINotEqual %bool %int_1 %int_0
// CHECK-NEXT:    [[arr2bool1:%\d+]] = OpCompositeConstruct %_arr_bool_uint_2 [[bool1]] [[bool1]]
// CHECK-NEXT:       [[arr5i1:%\d+]] = OpCompositeConstruct %_arr_int_uint_5 %int_1 %int_1 %int_1 %int_1 %int_1
// CHECK-NEXT:         [[f1_1:%\d+]] = OpConvertSToF %float %int_1
// CHECK-NEXT:         [[col3:%\d+]] = OpCompositeConstruct %v3float [[f1_1]] [[f1_1]] [[f1_1]]
// CHECK-NEXT:    [[matFloat1:%\d+]] = OpCompositeConstruct %mat2v3float [[col3]] [[col3]]
// CHECK-NEXT:         [[v3i1:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_1 %int_1
// CHECK-NEXT:       [[intmat:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[v3i1]] [[v3i1]]
// CHECK-NEXT:         [[true:%\d+]] = OpINotEqual %bool %int_1 %int_0
// CHECK-NEXT:      [[boolvec:%\d+]] = OpCompositeConstruct %v3bool [[true]] [[true]] [[true]]
// CHECK-NEXT:      [[boolmat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolvec]] [[boolvec]]
// CHECK-NEXT: [[flatConvert1:%\d+]] = OpCompositeConstruct %VSOutput [[v4f1]] [[v3u1]] [[v2i1]] [[arr2bool1]] [[arr5i1]] [[matFloat1]] [[intmat]] [[boolmat]]
// CHECK-NEXT:                         OpStore %output4 [[flatConvert1]]
  VSOutput output4 = (VSOutput)1;

// CHECK:                [[x:%\d+]] = OpLoad %int %x
// CHECK-NEXT:       [[floatX:%\d+]] = OpConvertSToF %float [[x]]
// CHECK-NEXT:         [[v4fX:%\d+]] = OpCompositeConstruct %v4float [[floatX]] [[floatX]] [[floatX]] [[floatX]]
// CHECK-NEXT:        [[uintX:%\d+]] = OpBitcast %uint [[x]]
// CHECK-NEXT:         [[v3uX:%\d+]] = OpCompositeConstruct %v3uint [[uintX]] [[uintX]] [[uintX]]
// CHECK-NEXT:         [[v2iX:%\d+]] = OpCompositeConstruct %v2int [[x]] [[x]]
// CHECK-NEXT:        [[boolX:%\d+]] = OpINotEqual %bool [[x]] %int_0
// CHECK-NEXT:    [[arr2boolX:%\d+]] = OpCompositeConstruct %_arr_bool_uint_2 [[boolX]] [[boolX]]
// CHECK-NEXT:       [[arr5iX:%\d+]] = OpCompositeConstruct %_arr_int_uint_5 [[x]] [[x]] [[x]] [[x]] [[x]]
// CHECK-NEXT:      [[floatX2:%\d+]] = OpConvertSToF %float [[x]]
// CHECK-NEXT:         [[v3fX:%\d+]] = OpCompositeConstruct %v3float [[floatX2]] [[floatX2]] [[floatX2]]
// CHECK-NEXT:    [[matFloatX:%\d+]] = OpCompositeConstruct %mat2v3float [[v3fX]] [[v3fX]]
// CHECK-NEXT:       [[intvec:%\d+]] = OpCompositeConstruct %v3int [[x]] [[x]] [[x]]
// CHECK-NEXT:       [[intmat:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[intvec]] [[intvec]]
// CHECK-NEXT:        [[boolx:%\d+]] = OpINotEqual %bool [[x]] %int_0
// CHECK-NEXT:      [[boolvec:%\d+]] = OpCompositeConstruct %v3bool [[boolx]] [[boolx]] [[boolx]]
// CHECK-NEXT:      [[boolmat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolvec]] [[boolvec]]
// CHECK-NEXT: [[flatConvert2:%\d+]] = OpCompositeConstruct %VSOutput [[v4fX]] [[v3uX]] [[v2iX]] [[arr2boolX]] [[arr5iX]] [[matFloatX]] [[intmat]] [[boolmat]]
// CHECK-NEXT:                         OpStore %output5 [[flatConvert2]]
  VSOutput output5 = (VSOutput)x;

// CHECK:            [[v4f1_5:%\d+]] = OpCompositeConstruct %v4float %float_1_5 %float_1_5 %float_1_5 %float_1_5
// CHECK-NEXT:         [[u1_5:%\d+]] = OpConvertFToU %uint %float_1_5
// CHECK-NEXT:       [[v3u1_5:%\d+]] = OpCompositeConstruct %v3uint [[u1_5]] [[u1_5]] [[u1_5]]
// CHECK-NEXT:         [[i1_5:%\d+]] = OpConvertFToS %int %float_1_5
// CHECK-NEXT:       [[v2i1_5:%\d+]] = OpCompositeConstruct %v2int [[i1_5]] [[i1_5]]
// CHECK-NEXT:      [[bool1_5:%\d+]] = OpFOrdNotEqual %bool %float_1_5 %float_0
// CHECK-NEXT: [[arr2bool_1_5:%\d+]] = OpCompositeConstruct %_arr_bool_uint_2 [[bool1_5]] [[bool1_5]]
// CHECK-NEXT:         [[i1_5:%\d+]] = OpConvertFToS %int %float_1_5
// CHECK-NEXT:     [[arr5i1_5:%\d+]] = OpCompositeConstruct %_arr_int_uint_5 [[i1_5]] [[i1_5]] [[i1_5]] [[i1_5]] [[i1_5]]
// CHECK-NEXT:      [[v3f_1_5:%\d+]] = OpCompositeConstruct %v3float %float_1_5 %float_1_5 %float_1_5
// CHECK-NEXT: [[matFloat_1_5:%\d+]] = OpCompositeConstruct %mat2v3float [[v3f_1_5]] [[v3f_1_5]]
// CHECK-NEXT:      [[int_1_5:%\d+]] = OpConvertFToS %int %float_1_5
// CHECK-NEXT:       [[intvec:%\d+]] = OpCompositeConstruct %v3int [[int_1_5]] [[int_1_5]] [[int_1_5]]
// CHECK-NEXT:       [[intmat:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[intvec]] [[intvec]]
// CHECK-NEXT:     [[bool_1_5:%\d+]] = OpFOrdNotEqual %bool %float_1_5 %float_0
// CHECK-NEXT:      [[boolvec:%\d+]] = OpCompositeConstruct %v3bool [[bool_1_5]] [[bool_1_5]] [[bool_1_5]]
// CHECK-NEXT:      [[boolmat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolvec]] [[boolvec]]
// CHECK-NEXT:              {{%\d+}} = OpCompositeConstruct %VSOutput [[v4f1_5]] [[v3u1_5]] [[v2i1_5]] [[arr2bool_1_5]] [[arr5i1_5]] [[matFloat_1_5]] [[intmat]] [[boolmat]]
  VSOutput output6 = (VSOutput)1.5;

// CHECK:      [[float_true:%\d+]] = OpSelect %float %true %float_1 %float_0
// CHECK-NEXT:   [[v4f_true:%\d+]] = OpCompositeConstruct %v4float [[float_true]] [[float_true]] [[float_true]] [[float_true]]
// CHECK-NEXT:  [[uint_true:%\d+]] = OpSelect %uint %true %uint_1 %uint_0
// CHECK-NEXT:   [[v3u_true:%\d+]] = OpCompositeConstruct %v3uint [[uint_true]] [[uint_true]] [[uint_true]]
// CHECK-NEXT:   [[int_true:%\d+]] = OpSelect %int %true %int_1 %int_0
// CHECK-NEXT:   [[v2i_true:%\d+]] = OpCompositeConstruct %v2int [[int_true]] [[int_true]]
// CHECK-NEXT:  [[arr2_true:%\d+]] = OpCompositeConstruct %_arr_bool_uint_2 %true %true
// CHECK-NEXT:   [[int_true:%\d+]] = OpSelect %int %true %int_1 %int_0
// CHECK-NEXT: [[arr5i_true:%\d+]] = OpCompositeConstruct %_arr_int_uint_5 [[int_true]] [[int_true]] [[int_true]] [[int_true]] [[int_true]]
// CHECK-NEXT: [[float_true:%\d+]] = OpSelect %float %true %float_1 %float_0
// CHECK-NEXT:   [[v3f_true:%\d+]] = OpCompositeConstruct %v3float [[float_true]] [[float_true]] [[float_true]]
// CHECK-NEXT:[[mat2v3_true:%\d+]] = OpCompositeConstruct %mat2v3float [[v3f_true]] [[v3f_true]]
// CHECK-NEXT:   [[true_int:%\d+]] = OpSelect %int %true %int_1 %int_0
// CHECK-NEXT:     [[intvec:%\d+]] = OpCompositeConstruct %v3int [[true_int]] [[true_int]] [[true_int]]
// CHECK-NEXT:     [[intmat:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[intvec]] [[intvec]]
// CHECK-NEXT:    [[boolvec:%\d+]] = OpCompositeConstruct %v3bool %true %true %true
// CHECK-NEXT:    [[boolmat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolvec]] [[boolvec]]
// CHECK-NEXT:            {{%\d+}} = OpCompositeConstruct %VSOutput [[v4f_true]] [[v3u_true]] [[v2i_true]] [[arr2_true]] [[arr5i_true]] [[mat2v3_true]] [[intmat]] [[boolmat]]
  VSOutput output7 = (VSOutput)true;

}
