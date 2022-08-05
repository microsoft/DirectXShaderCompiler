// RUN: %dxc -T ps_6_1 -E main

// CHECK: OpCapability FragmentBarycentricKHR
// CHECK: OpExtension "SPV_KHR_fragment_shader_barycentric"
enum VertexID { 
    FIRST = 0,
    SECOND = 1,
    THIRD = 2
};

float3 main( float3 vBaryWeights : SV_Barycentrics,
	 nointerpolation float3 Color : COLOR ) : SV_Target
{
    // CHECK: OpDecorate [[bc:%\d+]] BuiltIn BaryCoordKHR
    // CHECK-NEXT: OpDecorate %in_var_COLOR Flat
    // CHECK-NEXT: OpDecorate %in_var_COLOR PerVertexKHR
    float3 vColor;

    // CHECK: %in_var_COLOR = OpVariable %_ptr_Input__arr_v3float_uint_3 Input
    // CHECK: %param_var_Color = OpVariable %_ptr_Function__arr_v3float_uint_3 Function
    // CHECK: [[bcl:%\d+]] = OpLoad %v3float [[bc]]
    // CHECK: OpStore %param_var_vBaryWeights [[bcl]]
    // CHECK: [[ivC:%\d+]] = OpLoad %_arr_v3float_uint_3 %in_var_COLOR
    // CHECK: OpStore %param_var_Color [[ivC]]
    float3 vColor0 = GetAttributeAtVertex( Color, VertexID::FIRST );
    float3 vColor1 = GetAttributeAtVertex( Color, VertexID::SECOND );
    float3 vColor2 = GetAttributeAtVertex( Color, VertexID::THIRD );

    // CHECK: %Color = OpFunctionParameter %_ptr_Function_v3float
    // CHECK: [[C0:%\d+]] = OpAccessChain %_ptr_Function_v3float %Color %uint_0
    // CHECK: [[C0L:%\d+]] = OpLoad %v3float [[C0]]
    // CHECK: OpStore %vColor0 [[C0L]]
    vColor = vBaryWeights.x*vColor0 + vBaryWeights.y*vColor1 + vBaryWeights.z*vColor2;

    return vColor;
}
