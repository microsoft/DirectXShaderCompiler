// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};

struct PSInput
{
    bool3 color0 : COLOR0;
    nointerpolation bool3 color1 : COLOR1;
    nointerpolation bool3 color2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color0:%\w+]] Location 0
// CHECK: OpDecorate [[color1:%\w+]] Location 1
// CHECK: OpDecorate [[color2:%\w+]] Location 4
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %v3bool %_arr_v3bool_uint_3 %_arr_v3bool_uint_3
// CHECK: [[color0]] = OpVariable %_ptr_Input_v3uint Input
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
bool3 main(PSInput input) : SV_Target
{
    bool3 vColor0 = input.color0;
    bool3 vColor1 = input.color1;
    bool3 vColor2 = GetAttributeAtVertex(input.color1, VertexID::SECOND );
    bool3 vColor3 = input.color1;
    bool3 vColor4 = bool3(input.color2.y, input.color1.z, vColor2.y);
    return and(and(and(and(vColor0, vColor1), vColor2), vColor3), vColor4);
}
// CHECK: [[input:%\w+]] = OpFunctionParameter [[Ptr_PSInput:%\w+]]
// CHECK: [[acInputColor2:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[input]] %int_2 %uint_0
// CHECK: [[color2Y:%\d+]] = OpAccessChain %_ptr_Function_bool [[acInputColor2]] %int_1
// CHECK: [[loadc2y:%\d+]] = OpLoad %bool [[color2Y]]
// CHECK: [[result:%\d+]] = OpCompositeConstruct %v3bool [[loadc2y]] [[loadc1z:%\d+]] [[vc2y:%\d+]]