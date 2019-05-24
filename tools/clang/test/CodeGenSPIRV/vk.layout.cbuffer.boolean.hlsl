// Run: %dxc -T ps_6_0 -E main

// CHECK: OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
// CHECK: OpMemberDecorate %FrameConstants 0 Offset 0
// CHECK: OpMemberDecorate %FrameConstants 1 Offset 4
// CHECK: OpMemberDecorate %FrameConstants 2 Offset 16
// CHECK: OpMemberDecorate %type_CONSTANTS 0 Offset 0
// CHECK: OpDecorate %type_CONSTANTS Block

// CHECK: [[v3uint0:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
// CHECK: [[v2uint0:%\d+]] = OpConstantComposite %v2uint %uint_0 %uint_0

// CHECK: %T = OpTypeStruct %_arr_uint_uint_1
struct T {
  bool boolArray[1];
};

// CHECK: %FrameConstants = OpTypeStruct %uint %v3uint %_arr_v3uint_uint_2 %T
struct FrameConstants
{
  bool  boolScalar;
  bool3 boolVec;
  row_major bool2x3 boolMat;
  T t;
};

[[vk::binding(0, 0)]]
cbuffer CONSTANTS
{
  FrameConstants frameConstants;
};

// These are the types that hold SPIR-V booleans, rather than Uints.
// CHECK:              %T_0 = OpTypeStruct %_arr_bool_uint_1
// CHECK: %FrameConstants_0 = OpTypeStruct %bool %v3bool %_arr_v3bool_uint_2 %T_0

float4 main(in float4 texcoords : TEXCOORD0) : SV_TARGET
{
// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint [[FrameConstants]] %int_1
// CHECK-NEXT:        [[uintVec:%\d+]] = OpLoad %v3uint [[uintVecPtr]]
// CHECK-NEXT:        [[boolVec:%\d+]] = OpINotEqual %v3bool [[uintVec]] [[v3uint0]]
// CHECK-NEXT:                           OpStore %a [[boolVec]]
    bool3   a = frameConstants.boolVec;

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[FrameConstants]] %int_1 %uint_0
// CHECK-NEXT:           [[uint:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:           [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK-NEXT:                           OpStore %b [[bool]]
    bool    b = frameConstants.boolVec[0];

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[FrameConstants]] %int_0
// CHECK-NEXT:           [[uint:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:           [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK-NEXT:                           OpStore %c [[bool]]
    bool    c = frameConstants.boolScalar;

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintMatPtr:%\d+]] = OpAccessChain %_ptr_Uniform__arr_v3uint_uint_2 [[FrameConstants]] %int_2
// CHECK-NEXT:        [[uintMat:%\d+]] = OpLoad %_arr_v3uint_uint_2 [[uintMatPtr]]
// CHECK-NEXT:       [[uintVec1:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 0
// CHECK-NEXT:       [[boolVec1:%\d+]] = OpINotEqual %v3bool [[uintVec1]] [[v3uint0]]
// CHECK-NEXT:       [[uintVec2:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 1
// CHECK-NEXT:       [[boolVec2:%\d+]] = OpINotEqual %v3bool [[uintVec2]] [[v3uint0]]
// CHECK-NEXT:        [[boolMat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolVec1]] [[boolVec2]]
// CHECK-NEXT:                           OpStore %d [[boolMat]]
    bool2x3 d = frameConstants.boolMat;

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint [[FrameConstants]] %int_2 %uint_0
// CHECK-NEXT:        [[uintVec:%\d+]] = OpLoad %v3uint [[uintVecPtr]]
// CHECK-NEXT:        [[boolVec:%\d+]] = OpINotEqual %v3bool [[uintVec]] [[v3uint0]]
// CHECK-NEXT:                           OpStore %e [[boolVec]]
    bool3   e = frameConstants.boolMat[0];

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[FrameConstants]] %int_2 %uint_1 %uint_2
// CHECK-NEXT:           [[uint:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:           [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK-NEXT:                           OpStore %f [[bool]]
    bool    f = frameConstants.boolMat[1][2];

// Swizzle Vector: out of order
// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint [[FrameConstants]] %int_1
// CHECK-NEXT:        [[uintVec:%\d+]] = OpLoad %v3uint [[uintVecPtr]]
// CHECK-NEXT:        [[boolVec:%\d+]] = OpINotEqual %v3bool [[uintVec]] [[v3uint0]]
// CHECK-NEXT:        [[swizzle:%\d+]] = OpVectorShuffle %v2bool [[boolVec]] [[boolVec]] 1 0
// CHECK-NEXT:                           OpStore %g [[swizzle]]
    bool2   g = frameConstants.boolVec.yx;

// Swizzle Vector: one element only.
// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint [[FrameConstants]] %int_1
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[uintVecPtr]] %int_0
// CHECK-NEXT:           [[uint:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:           [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK-NEXT:                           OpStore %h [[bool]]
    bool    h = frameConstants.boolVec.x;

// Swizzle Vector: original indeces.
// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint [[FrameConstants]] %int_1
// CHECK-NEXT:        [[uintVec:%\d+]] = OpLoad %v3uint [[uintVecPtr]]
// CHECK-NEXT:        [[boolVec:%\d+]] = OpINotEqual %v3bool [[uintVec]] [[v3uint0]]
// CHECK-NEXT:                           OpStore %i [[boolVec]]
    bool3   i = frameConstants.boolVec.xyz;

// Swizzle Vector: on temporary value (rvalue)
// CHECK:       [[uintVec1:%\d+]] = OpLoad %v3uint {{%\d+}}
// CHECK-NEXT:  [[boolVec1:%\d+]] = OpINotEqual %v3bool [[uintVec1]] [[v3uint0]]
// CHECK:       [[uintVec2:%\d+]] = OpLoad %v3uint {{%\d+}}
// CHECK-NEXT:  [[boolVec2:%\d+]] = OpINotEqual %v3bool [[uintVec2]] [[v3uint0]]
// CHECK-NEXT: [[temporary:%\d+]] = OpLogicalAnd %v3bool [[boolVec1]] [[boolVec2]]
// CHECK-NEXT:      [[bool:%\d+]] = OpCompositeExtract %bool [[temporary]] 0
// CHECK-NEXT:                      OpStore %j [[bool]]
    bool    j = (frameConstants.boolVec && frameConstants.boolVec).x;

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:     [[uintMatPtr:%\d+]] = OpAccessChain %_ptr_Uniform__arr_v3uint_uint_2 [[FrameConstants]] %int_2
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[uintMatPtr]] %int_1 %int_2
// CHECK-NEXT:          [[uint0:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:        [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[uintMatPtr]] %int_0 %int_1
// CHECK-NEXT:          [[uint1:%\d+]] = OpLoad %uint [[uintPtr]]
// CHECK-NEXT:        [[uintVec:%\d+]] = OpCompositeConstruct %v2uint [[uint0]] [[uint1]]
// CHECK-NEXT:        [[boolVec:%\d+]] = OpINotEqual %v2bool [[uintVec]] [[v2uint0]]
// CHECK-NEXT:                           OpStore %k [[boolVec]]
    bool2   k = frameConstants.boolMat._m12_m01;

// CHECK:      [[FrameConstants:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:            [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[FrameConstants]] %int_3 %int_0 %int_2
// CHECK-NEXT:           [[uint:%\d+]] = OpLoad %uint [[ptr]]
// CHECK-NEXT:           [[bool:%\d+]] = OpINotEqual %bool [[uint]] %uint_0
// CHECK-NEXT:                           OpStore %l [[bool]]
    bool    l = frameConstants.t.boolArray[2];

// CHECK:           [[FrameConstantsPtr:%\d+]] = OpAccessChain %_ptr_Uniform_FrameConstants %CONSTANTS %int_0
// CHECK-NEXT:         [[FrameConstants:%\d+]] = OpLoad %FrameConstants [[FrameConstantsPtr]]
// CHECK-NEXT:              [[fc_0_uint:%\d+]] = OpCompositeExtract %uint [[FrameConstants]] 0
// CHECK-NEXT:              [[fc_0_bool:%\d+]] = OpINotEqual %bool [[fc_0_uint]] %uint_0
// CHECK-NEXT:             [[fc_1_uint3:%\d+]] = OpCompositeExtract %v3uint [[FrameConstants]] 1
// CHECK-NEXT:             [[fc_1_bool3:%\d+]] = OpINotEqual %v3bool [[fc_1_uint3]] [[v3uint0]]
// CHECK-NEXT:           [[fc_2_uintMat:%\d+]] = OpCompositeExtract %_arr_v3uint_uint_2 [[FrameConstants]] 2
// CHECK-NEXT: [[fc_2_uintMat_row0_uint:%\d+]] = OpCompositeExtract %v3uint [[fc_2_uintMat]] 0
// CHECK-NEXT: [[fc_2_uintMat_row0_bool:%\d+]] = OpINotEqual %v3bool [[fc_2_uintMat_row0_uint]] [[v3uint0]]
// CHECK-NEXT: [[fc_2_uintMat_row1_uint:%\d+]] = OpCompositeExtract %v3uint [[fc_2_uintMat]] 1
// CHECK-NEXT: [[fc_2_uintMat_row1_bool:%\d+]] = OpINotEqual %v3bool [[fc_2_uintMat_row1_uint]] [[v3uint0]]
// CHECK-NEXT:           [[fc_2_boolMat:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[fc_2_uintMat_row0_bool]] [[fc_2_uintMat_row1_bool]]
// CHECK-NEXT:                 [[fc_3_T:%\d+]] = OpCompositeExtract %T [[FrameConstants]] 3
// CHECK-NEXT:      [[fc_3_T_0_uint_arr:%\d+]] = OpCompositeExtract %_arr_uint_uint_1 [[fc_3_T]] 0
// CHECK-NEXT:        [[fc_3_T_0_0_uint:%\d+]] = OpCompositeExtract %uint [[fc_3_T_0_uint_arr]] 0
// CHECK-NEXT:        [[fc_3_T_0_0_bool:%\d+]] = OpINotEqual %bool [[fc_3_T_0_0_uint]] %uint_0
// CHECK-NEXT:      [[fc_3_T_0_bool_arr:%\d+]] = OpCompositeConstruct %_arr_bool_uint_1 [[fc_3_T_0_0_bool]]
// CHECK-NEXT:            [[fc_3_T_bool:%\d+]] = OpCompositeConstruct %T_0 [[fc_3_T_0_bool_arr]]
// CHECK-NEXT:                     [[fc:%\d+]] = OpCompositeConstruct %FrameConstants_0 [[fc_0_bool]] [[fc_1_bool3]] [[fc_2_boolMat]] [[fc_3_T_bool]]
// CHECK-NEXT:                                   OpStore %fc [[fc]]
    FrameConstants fc = frameConstants;

    return (1.0).xxxx;
}
