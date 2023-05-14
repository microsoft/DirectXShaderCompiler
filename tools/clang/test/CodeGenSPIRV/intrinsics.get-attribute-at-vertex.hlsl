// RUN: %dxc -T ps_6_1 -E main

enum VertexID {
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};


// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
// CHECK: OpDecorate [[ivc:%\w+]] Flat
// CHECK: OpDecorate [[ivc]] PerVertexKHR
// CHECK: OpDecorate [[ivc2:%\w+]] Flat
// CHECK: OpDecorate [[ivc2]] PerVertexKHR
// CHECK: [[ivc]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[ivc2]] = OpVariable %_ptr_Input__arr_v3uint_uint_3 Input
// CHECK: [[pvc:%\w+]] = OpVariable %_ptr_Function__arr_v3float_uint_3 Function
// CHECK: [[pvc2:%\w+]] = OpVariable %_ptr_Function__arr_v3uint_uint_3 Function
// CHECK: [[ivcl:%\d+]] = OpLoad %_arr_v3float_uint_3 [[ivc]]
// CHECK: OpStore [[pvc]] [[ivcl]]
// CHECK: [[ivcl2:%\d+]] = OpLoad %_arr_v3uint_uint_3 [[ivc2]]
// CHECK: OpStore [[pvc2]] [[ivcl2]]
float3 main( float3 vBaryWeights : SV_Barycentrics,
    nointerpolation float3 Color : COLOR,
    nointerpolation bool3 Color2 : COLOR2    ) : SV_Target
{
    float3 vColor;
    float3 vColor0 = GetAttributeAtVertex( Color, VertexID::FIRST );
    float3 vColor1 = GetAttributeAtVertex( Color, VertexID::SECOND );
    float3 vColor2 = GetAttributeAtVertex( Color2, VertexID::THIRD );
    vColor = vBaryWeights.x*vColor0 + vBaryWeights.y*vColor1 + vColor2;
    return vColor;
}

// CHECK: [[color:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3float_uint_3
// CHECK: [[color2:%\w+]] = OpFunctionParameter %_ptr_Function__arr_v3uint_uint_3
// CHECK: [[c0:%\w+]] = OpAccessChain %_ptr_Function_v3float [[color]] %uint_0
// CHECK: [[c0l:%\w+]] = OpLoad %v3float [[c0]]
// CHECK: OpStore [[vC0:%\w+]] [[c0l]]
// CHECK: [[c1:%\w+]] = OpAccessChain %_ptr_Function_v3uint [[color2]] %uint_2
// CHECK: [[c1load:%\w+]] = OpLoad %v3uint [[c1]]
// CHECK: [[c1Ne:%\w+]] = OpINotEqual %v3bool [[c1load]] [[constZeroUint:%\w+]]
// CHECK: [[c1Sel:%\w+]] = OpSelect %v3float [[c1Ne]] [[constOneTarget:%\w+]] [[constZeroTarget:%\w+]]
