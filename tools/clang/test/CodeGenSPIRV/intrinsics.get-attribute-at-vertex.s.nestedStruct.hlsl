// RUN: %dxc -T ps_6_1 -E main -spirv

enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};
struct PSInput1
{
    float3 color3: COLOR3;
};

struct PSInput
{
    float4 position : SV_POSITION;
    nointerpolation float3 color1 : COLOR1;
    nointerpolation PSInput1 pi : COLOR2;
};

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[color1:%\w+]] Location 0
// CHECK: OpDecorate [[color2:%\w+]] Location 3
// CHECK: OpDecorate [[color1]] PerVertexKHR
// CHECK: OpDecorate [[color2]] PerVertexKHR
// CHECK: [[PSInput1:%\w+]] = OpTypeStruct %_arr_v3float_uint_3
// CHECK: [[PSInput:%\w+]] = OpTypeStruct %v4float %_arr_v3float_uint_3 [[PSInput1]]
// CHECK: [[color1]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[color2]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[loadc1:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color1]]
// CHECK: [[loadc2:%\d+]] = OpLoad %_arr_v3float_uint_3 [[color2]]
// CHECK: [[loadpi1:%\d+]] = OpCompositeConstruct [[PSInput1]] [[loadc2]]
// CHECK: [[loadpi:%\d+]] = OpCompositeConstruct [[PSInput]] [[frag:%\d+]] [[loadc1]] [[loadpi1]]
float3 main( PSInput input ) : SV_Target
{
    float3 vColor0 = input.color1;
    float3 vColor1 = GetAttributeAtVertex( input.color1, VertexID::SECOND );
    float3 vColor2 = input.pi.color3;
    float3 vColor3 = GetAttributeAtVertex( input.pi.color3, VertexID::THIRD );
    return (vColor0 + vColor1 + vColor2 + vColor3);
}

// CHECK: [[input:%\w+]] = OpFunctionParameter %_ptr_Function_PSInput
// CHECK: %vColor2 = OpVariable %_ptr_Function_v3float Function
// CHECK: [[acColor3:%\d+]] = OpAccessChain %_ptr_Function_v3float [[input]] %int_2 %int_0 %uint_0
// CHECK: [[loadColor3:%\d+]] = OpLoad %v3float [[acColor3]]
// CHECK: OpStore %vColor2 [[loadColor3]]