// Run: %dxc -T ps_6_0 -E main

struct VSOutput {
  float4   sv_pos     : SV_POSITION;
  uint3    normal     : NORMAL;
  int2     tex_coord  : TEXCOORD;
  bool     mybool[2]  : MYBOOL;
  int      arr[5]     : MYARRAY;
  float2x3 mat2x3     : MYMATRIX;
};


// CHECK: [[nullVSOutput:%\d+]] = OpConstantNull %VSOutput


void main() {
  int x = 3;

// CHECK: OpStore %output1 [[nullVSOutput]]
  VSOutput output1 = (VSOutput)0;

// TODO: Avoid OpBitCast from 'literal int' to 'int'
//
// CHECK:                [[f1:%\d+]] = OpConvertSToF %float %int_1
// CHECK-NEXT:         [[v4f1:%\d+]] = OpCompositeConstruct %v4float [[f1]] [[f1]] [[f1]] [[f1]]
// CHECK-NEXT:           [[u1:%\d+]] = OpBitcast %uint %int_1
// CHECK-NEXT:         [[v3u1:%\d+]] = OpCompositeConstruct %v3uint [[u1]] [[u1]] [[u1]]
// CHECK-NEXT:         [[i1_0:%\d+]] = OpBitcast %int %int_1
// CHECK-NEXT:         [[v2i1:%\d+]] = OpCompositeConstruct %v2int [[i1_0]] [[i1_0]]
// CHECK-NEXT:        [[bool1:%\d+]] = OpINotEqual %bool %int_1 %int_0
// CHECK-NEXT:    [[arr2bool1:%\d+]] = OpCompositeConstruct %_arr_bool_uint_2 [[bool1]] [[bool1]]
// CHECK-NEXT:         [[i1_1:%\d+]] = OpBitcast %int %int_1
// CHECK-NEXT:       [[arr5i1:%\d+]] = OpCompositeConstruct %_arr_int_uint_5 [[i1_1]] [[i1_1]] [[i1_1]] [[i1_1]] [[i1_1]]
// CHECK-NEXT:         [[f1_1:%\d+]] = OpConvertSToF %float %int_1
// CHECK-NEXT:         [[col3:%\d+]] = OpCompositeConstruct %v3float [[f1_1]] [[f1_1]] [[f1_1]]
// CHECK-NEXT:    [[matFloat1:%\d+]] = OpCompositeConstruct %mat2v3float [[col3]] [[col3]]
// CHECK-NEXT: [[flatConvert1:%\d+]] = OpCompositeConstruct %VSOutput [[v4f1]] [[v3u1]] [[v2i1]] [[arr2bool1]] [[arr5i1]] [[matFloat1]]
// CHECK-NEXT:                         OpStore %output2 [[flatConvert1]]
  VSOutput output2 = (VSOutput)1;

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
// CHECK-NEXT: [[flatConvert2:%\d+]] = OpCompositeConstruct %VSOutput [[v4fX]] [[v3uX]] [[v2iX]] [[arr2boolX]] [[arr5iX]] [[matFloatX]]
// CHECK-NEXT:                         OpStore %output3 [[flatConvert2]]
  VSOutput output3 = (VSOutput)x;
}
