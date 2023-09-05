// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


struct PSInput
{
    nointerpolation float3 color0 : COLOR0;
    nointerpolation float3 color1 : COLOR1;
    nointerpolation bool3 color2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color0:%\w+]] Location 0
// CHECK: OpDecorate [[color1:%\w+]] Location 3
// CHECK: OpDecorate [[color2:%\w+]] Location 6
// CHECK: OpDecorate [[color0]] PerVertexKHR
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %_arr_v3float_uint_3 %_arr_v3float_uint_3 %_arr_v3bool_uint_3
// CHECK: [[color0]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[outTarget0:%\w+]] = OpVariable %_ptr_Output_v3float Output
// CHECK: [[outTarget1:%\w+]] = OpVariable %_ptr_Output_v3float Output
// CHECK: [[outTarget2:%\w+]] = OpVariable %_ptr_Output_v3uint Output
// CHECK: [[color2BoolArr:%\w+]] = OpVariable %_ptr_Function__arr_v3bool_uint_3 Function
// CHECK: [[in3:%\d+]] = OpLoad %_arr_v3bool_uint_3 [[color2BoolArr]]
// CHECK: [[in1:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color0]]
// CHECK: [[in2:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color1]]
// CHECK: [[inputVar:%\d+]] = OpCompositeConstruct [[PSInput]] [[in1]] [[in2]] [[in3]]
// CHECK: [[out1:%\d+]] = OpCompositeExtract %v3float [[mainRet:%\d+]] 1 0
// CHECK: OpStore [[outTarget1]] [[out1]]
PSInput main( PSInput input ) : SV_Target
{
    PSInput retVal;
    retVal.color0 = 0.1;
    retVal.color1 += input.color1;
    retVal.color1 += GetAttributeAtVertex( input.color1, VertexID::SECOND );
    retVal.color2 = input.color2;
    retVal.color2 = GetAttributeAtVertex( input.color2, VertexID::THIRD );
    return retVal;
}

// CHECK: [[input:%\w+]] = OpFunctionParameter [[Ptr_PSInput:%\w+]]
// CHECK: [[retVal:%\w+]] = OpVariable [[Ptr_PSInput]] Function
// CHECK: [[acInput2:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[input]] %int_2 %uint_0
// CHECK: [[loadInput2:%\d+]] = OpLoad %v3bool [[acInput2]]
// CHECK: [[storeRet:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[retVal]] %int_2 %uint_0
// CHECK: OpStore [[storeRet]] [[loadInput2]]