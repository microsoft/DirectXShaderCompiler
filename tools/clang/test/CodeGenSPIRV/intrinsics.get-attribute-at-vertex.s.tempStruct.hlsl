// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


struct PSInput
{
    float2 m0 : COLOR0;
    nointerpolation float3 m1 : COLOR1;
    nointerpolation bool3 m2 : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color4:%\w+]] Location 1
// CHECK: OpDecorate [[color5:%\w+]] Location 4
// CHECK: OpDecorate [[color4]] PerVertexKHR
// CHECK: OpDecorate [[color5]] PerVertexKHR
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %v2float %_arr_v3float_uint_3 %_arr_v3bool_uint_3
// CHECK: [[color4]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[color5]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[outTarget1:%\w+]] = OpVariable %_ptr_Output_v3float Output
// CHECK: [[outTarget2:%\w+]] = OpVariable %_ptr_Output_v3uint Output
// CHECK: %in_var_COLOR5_BoolArr_PerVertex = OpVariable %_ptr_Function__arr_v3bool_uint_3 Function
// CHECK: [[tmpvtx1:%\d+]] = OpAccessChain %_ptr_Function_v3bool %in_var_COLOR5_BoolArr_PerVertex
// CHECK: [[loadvtx1:%\d+]] = OpLoad %_arr_v3bool_uint_3 %in_var_COLOR5_BoolArr_PerVertex
// CHECK: [[outextract2:%\d+]] = OpCompositeExtract %v3float [[mainret:%\d+]] 1 0
// CHECK: OpStore [[outTarget1]] [[outextract2]]
PSInput main( float2 color0 : COLOR3,
             nointerpolation float3 color1 : COLOR4,
             nointerpolation bool3 color2 : COLOR5) : SV_Target
{
    PSInput retVal;
    retVal.m0 = color0;
    retVal.m1 += color1;
    retVal.m1 += GetAttributeAtVertex( color1, VertexID::SECOND );
    retVal.m2 = color2;
    retVal.m2 = GetAttributeAtVertex( color2, VertexID::THIRD );
    return retVal;
}

// CHECK: [[c2pvt0:%\w+]] = OpVariable %_ptr_Function_v3bool Function
// CHECK: [[c2e0:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[funcp2:%\w+]] %uint_0
// CHECK: [[loadc2e0:%\d+]] = OpLoad %v3bool [[c2e0]]
// CHECK: OpStore [[c2pvt0]] [[loadc2e0]]
// CHECK: [[ldc2pvt0:%\d+]] = OpLoad %v3bool [[c2pvt0]]
// CHECK: [[acRetVal2:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[retVal:%\w+]] %int_2 %uint_0
// CHECK: OpStore [[acRetVal2]] [[ldc2pvt0]]