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
// CHECK: [[ivc]] = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
// CHECK: [[pvc:%\w+]] = OpVariable %_ptr_Function__arr_v3float_uint_3 Function
// CHECK: [[ivcl:%\d+]] = OpLoad %_arr_v3float_uint_3 [[ivc]]
// CHECK: OpStore [[pvc]] [[ivcl]]
float3 main( float3 vBaryWeights : SV_Barycentrics,
    nointerpolation float3 Color : COLOR ) : SV_Target
{
    float3 vColor;
    float3 vColor0 = GetAttributeAtVertex( Color, VertexID::FIRST );
    float3 vColor1 = GetAttributeAtVertex( Color, VertexID::SECOND );
    vColor = vBaryWeights.x*vColor0 + vBaryWeights.y*vColor1;
    return vColor;
}

// CHECK: [[c0:%\d+]] = OpAccessChain %_ptr_Function_v3float [[color:%\w+]] %uint_0
// CHECK: [[c0l:%\d+]] = OpLoad %v3float [[c0]]
// CHECK: OpStore [[vC0:%\w+]] [[c0l]]