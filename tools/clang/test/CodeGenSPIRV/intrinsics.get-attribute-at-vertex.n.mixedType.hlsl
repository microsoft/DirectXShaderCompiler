// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color1:%\w+]] Location 0
// CHECK: OpDecorate [[color2:%\w+]] Location 3
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
float3 main(
    float4 position : SV_POSITION,
    nointerpolation bool3 color1 : COLOR1,
    nointerpolation float3 color2 : COLOR2) : SV_Target
{
    float3 vColor0 = color1;
    float3 vColor1 = GetAttributeAtVertex( color1, VertexID::SECOND );
    float3 vColor2 = color2;
    float3 vColor3 = GetAttributeAtVertex( color2, VertexID::THIRD );
    return (vColor0 + vColor1 + vColor2 + vColor3);
}
// CHECK: [[color1:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3bool_uint_3
// CHECK: [[vColor0:%\w+]] = OpVariable %_ptr_Function_v3float Function
// CHECK: [[c1Param0:%\w+]] = OpVariable %_ptr_Function_v3bool Function
// CHECK: [[inst80:%\d+]] = OpAccessChain %_ptr_Function_v3bool [[color1]] %uint_0
// CHECK: [[inst81:%\d+]] = OpLoad %v3bool [[inst80]]
// CHECK: OpStore [[c1Param0]] [[inst81]]
// CHECK: [[inst84:%\d+]] = OpLoad %v3bool [[c1Param0]]
// CHECK: [[inst85:%\d+]] = OpSelect %v3float [[inst84]] [[trueVec:%\d+]] [[falseVec:%\d+]]
// CHECK: OpStore [[vColor0]] [[inst85]]