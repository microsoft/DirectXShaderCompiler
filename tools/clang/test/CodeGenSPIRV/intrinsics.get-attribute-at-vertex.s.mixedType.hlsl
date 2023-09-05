// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};

struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation bool3 color1 : COLOR1;
    nointerpolation float3 color2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color1:%\w+]] Location 0
// CHECK: OpDecorate [[color2:%\w+]] Location 3
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %v4float %_arr_v3bool_uint_3 %_arr_v3float_uint_3
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[color2BoolArr:%\w+]] = OpVariable %_ptr_Function__arr_v3bool_uint_3 Function
// CHECK: [[inst47:%\d+]] = OpAccessChain %_ptr_Input_v3uint [[color1]] %uint_1
// CHECK: [[inst51:%\d+]] = OpLoad %v3uint [[inst47]]
// CHECK: [[inst52:%\d+]] = OpINotEqual %v3bool [[inst51]] [[vec3u0:%\d+]]
// CHECK: [[inst57:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[color2BoolArr]] %uint_1
// CHECK: OpStore [[inst57]] [[inst52]]
// CHECK: [[inst59:%\d+]] = OpLoad %_arr_v3bool_uint_3 [[color2BoolArr]]
// CHECK: [[inst63:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color2]]
// CHECK: [[inputParMain:%\d+]] = OpCompositeConstruct [[PSInput]] [[coord:%\d+]] [[inst59]] [[inst63]]
float3 main(PSInput input ) : SV_Target
{
    float3 vColor0 = input.color1;
    float3 vColor1 = GetAttributeAtVertex( input.color1, VertexID::SECOND );
    float3 vColor2 = input.color2;
    float3 vColor3 = GetAttributeAtVertex( input.color2, VertexID::THIRD );
    return (vColor0 + vColor1 + vColor2 + vColor3);
}
// CHECK: [[inputFuncParam:%\w+]] = OpFunctionParameter [[PSInputPtrType:%\w+]]
// CHECK: [[ins75:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[inputFuncParam]] %int_1 %uint_0
// CHECK: [[ins76:%\d+]] = OpLoad %v3bool [[ins75]]
// CHECK: [[ins77:%\d+]] = OpSelect %v3float [[ins76]] [[trueVec:%\d+]] [[falseVec:%\d+]]
// CHECK: OpStore [[vColor0:%\w+]] [[ins77]]