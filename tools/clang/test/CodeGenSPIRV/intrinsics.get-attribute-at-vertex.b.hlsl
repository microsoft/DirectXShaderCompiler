// RUN: %dxc -T ps_6_1 -E main

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};

struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float3 color1 : COLOR1;
    nointerpolation bool3 color2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[ivc:%\w+]] PerVertexKHR
// CHECK: OpDecorate [[ivc2:%\w+]] PerVertexKHR
// CHECK: [[ivc]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[ivc2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input

// CHECK: [[pvi:%\w+]] = OpVariable %_ptr_Function_PSInput Function
// CHECK: [[pvi1:%\d+]] = OpLoad %v4float [[fragColor:%\w+]]
// CHECK: [[pvi2:%\d+]] = OpLoad %_arr_v3float_uint_3 [[ivc]]
// CHECK: [[pvi3:%\d+]] = OpLoad %_arr_v3uint_uint_3 [[ivc2]]
// CHECK: [[pvitmp:%\d+]] = OpCompositeConstruct %PSInput [[pvi1]] [[pvi2]] [[pvi3]]
// CHECK: OpStore [[pvi]] [[pvitmp]]
// CHECK: [[pvicall:%\d+]] = OpFunctionCall %v3float %src_main [[pvi]]
float3 main(PSInput input ) : SV_Target
{
    float3 vColor0 = GetAttributeAtVertex( input.color1, VertexID::SECOND );
    float3 vColor1 = GetAttributeAtVertex( input.color2, VertexID::THIRD );
    return (vColor0 + vColor1);
}

// CHECK: [[input:%\w+]] = OpFunctionParameter %_ptr_Function_PSInput
// CHECK: [[color:%\d+]] = OpAccessChain %_ptr_Function__arr_v3float_uint_3 [[input]] %int_1
// CHECK: [[color0:%\d+]] = OpAccessChain %_ptr_Function_v3float [[color]] %uint_1
// CHECK: [[color1:%\d+]] = OpAccessChain %_ptr_Function__arr_v3uint_uint_3 [[input]] %int_2
// CHECK: [[color10:%\d+]] = OpAccessChain %_ptr_Function_v3uint [[color1]] %uint_2
// CHECK: [[result:%\d+]] = OpLoad %v3uint [[color10]]
// CHECK: [[resultEq:%\d+]] = OpINotEqual %v3bool [[result]] [[constZeroUint:%\d+]]
// CHECK: [[resultSlt:%\d+]] = OpSelect %v3float [[resultEq]] [[constOneDst:%\d+]] [[constZeroDst:%\d+]]