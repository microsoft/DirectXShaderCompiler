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
    nointerpolation bool3 color2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color1:%\w+]] Location 0
// CHECK: OpDecorate [[color2:%\w+]] Location 3
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %v4float %_arr_v3bool_uint_3 %_arr_v3bool_uint_3
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
bool3 main(PSInput input ) : SV_Target
{
    bool3 vColor0 = input.color1;
    bool3 vColor1 = GetAttributeAtVertex( input.color1, VertexID::SECOND );
    bool3 vColor2 = input.color2;
    bool3 vColor3 = GetAttributeAtVertex( input.color2, VertexID::THIRD );
    return and(and(and(vColor0, vColor1), vColor2), vColor3);
}
// CHECK: [[input:%\w+]] = OpFunctionParameter %_ptr_Function_PSInput
// CHECK: %vColor2 = OpVariable %_ptr_Function_v3bool Function
// CHECK: [[inst95:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[input]] %int_2 %uint_0
// CHECK: [[inst96:%\d+]] = OpLoad %v3bool [[inst95]]
// CHECK: OpStore %vColor2 [[inst96]]