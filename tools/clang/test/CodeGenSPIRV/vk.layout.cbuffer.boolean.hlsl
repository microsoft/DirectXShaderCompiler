// Run: %dxc -T ps_6_0 -E main

// CHECK: OpDecorate %_arr_v3uint_uint_2 ArrayStride 16
// CHECK: OpMemberDecorate %FrameConstants 0 Offset 0
// CHECK: OpMemberDecorate %FrameConstants 1 Offset 4
// CHECK: OpMemberDecorate %FrameConstants 2 Offset 16
// CHECK: OpMemberDecorate %type_CONSTANTS 0 Offset 0
// CHECK: OpDecorate %type_CONSTANTS Block

// CHECK: %FrameConstants = OpTypeStruct %uint %v3uint %_arr_v3uint_uint_2
struct FrameConstants
{
  bool  boolScalar;
  bool3 boolVec;
  row_major bool2x3 boolMat;
};

[[vk::binding(0, 0)]]
cbuffer CONSTANTS
{
  FrameConstants frameConstants;
};

// CHECK: [[v3uint0:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
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

    return (1.0).xxxx;
}
