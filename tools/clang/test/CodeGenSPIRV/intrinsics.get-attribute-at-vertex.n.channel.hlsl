// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[icolor0:%\w+]] Location 0
// CHECK: OpDecorate [[icolor1:%\w+]] Location 1
// CHECK: OpDecorate [[icolor2:%\w+]] Location 4
// CHECK: OpDecorate [[icolor1]] PerVertexKHR
// CHECK: OpDecorate [[icolor2]] PerVertexKHR
// CHECK: [[icolor0]] = OpVariable %_ptr_Input_v3uint Input
// CHECK: [[icolor1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[icolor2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
bool3 main(
    bool3 color0 : COLOR0,
    nointerpolation bool3 color1 : COLOR1,
    nointerpolation bool3 color2 : COLOR2) : SV_Target
{
    bool3 vColor0 = color0;
    bool3 vColor1 = color1;
    bool3 vColor2 = GetAttributeAtVertex( color1, VertexID::SECOND );
    bool3 vColor3 = color1;
    bool3 vColor4 = bool3(color2.y, color1.z, vColor2.y);
    return and(and(and(and(vColor0, vColor1), vColor2), vColor3), vColor4);
}        
// CHECK: [[color1:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3bool_uint_3
// CHECK: [[color2:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3bool_uint_3
// CHECK: [[inst104:%\d+]] = OpAccessChain %_ptr_Function_bool [[color2]] %uint_0 %int_1
// CHECK: [[inst105:%\d+]] = OpLoad %bool [[inst104]]
// CHECK: [[inst106:%\d+]] = OpAccessChain %_ptr_Function_bool [[color1]] %uint_0 %int_2
// CHECK: [[inst107:%\d+]] = OpLoad %bool [[inst106]]
// CHECK: [[inst108:%\d+]] = OpAccessChain %_ptr_Function_bool [[vColor2:%\w+]] %int_1
// CHECK: [[inst109:%\d+]] = OpLoad %bool [[inst108]]
// CHECK: [[inst110:%\d+]] = OpCompositeConstruct %v3bool [[inst105]] [[inst107]] [[inst109]]
// CHECK: OpStore [[vColor4:%\w+]] [[inst110]]