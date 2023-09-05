// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[icolor1:%\w+]] Location 0
// CHECK: OpDecorate [[icolor2:%\w+]] Location 3
// CHECK: OpDecorate [[icolor1]] PerVertexKHR
// CHECK: OpDecorate [[icolor2]] PerVertexKHR
// CHECK: [[icolor1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[icolor2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
bool3 main(
    float4 position : SV_POSITION,
    nointerpolation bool3 color1 : COLOR1,
    nointerpolation bool3 color2 : COLOR2) : SV_Target
{
    bool3 vColor0 = color1;
    bool3 vColor1 = GetAttributeAtVertex( color1, VertexID::SECOND );
    bool3 vColor2 = color2;
    bool3 vColor3 = GetAttributeAtVertex( color2, VertexID::THIRD );
    return and(and(and(vColor0 ,vColor1), vColor2), vColor3);
}
// CHECK: %in_var_COLOR1_BoolArr_PerVertex = OpVariable %_ptr_Function__arr_v3bool_uint_3 Function
// CHECK: [[inst48:%\d+]] = OpAccessChain %_ptr_Input_v3uint [[icolor1]] %uint_1
// CHECK: [[inst52:%\d+]] = OpLoad %v3uint [[inst48]]
// CHECK: [[inst53:%\d+]] = OpINotEqual %v3bool [[inst52]] [[inst13:%\d+]]
// CHECK: [[inst58:%\d+]] = OpAccessChain %_ptr_Function_v3bool %in_var_COLOR1_BoolArr_PerVertex %uint_1
// CHECK: OpStore [[inst58]] [[inst53]]
// CHECK: [[inst60:%\d+]] = OpLoad %_arr_v3bool_uint_3 %in_var_COLOR1_BoolArr_PerVertex
// CHECK: OpStore [[param_var_color1:%\w+]] [[inst60]]
// CHECK: [[inst79:%\d+]] = OpFunctionCall %v3bool [[src_main:%\w+]] [[param_var_position:%\w+]] [[param_var_color1]] [[param_var_color2:%\w+]]
// CHECK: [[color1:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3bool_uint_3
// CHECK: %color1_perVertexParam0 = OpVariable %_ptr_Function_v3bool Function
// CHECK: [[inst93:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[color1]] %uint_0
// CHECK: [[inst94:%\d+]] = OpLoad %v3bool [[inst93]]
// CHECK: OpStore %color1_perVertexParam0 [[inst94]]
