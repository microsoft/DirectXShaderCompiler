// RUN: %dxc -T ps_6_1 -E main

// CHECK:      OpCapability FragmentBarycentricKHR
// CHECK:      OpExtension "SPV_KHR_fragment_shader_barycentric"

// CHECK:      OpEntryPoint Fragment
// CHECK-SAME: [[bary:%\d+]]

// CHECK:      OpDecorate [[bary]] BuiltIn BaryCoordKHR
// CHECK:      OpDecorate [[bary]] Centroid
// CHECK:      [[bary]] = OpVariable %_ptr_Input_v3float Input

float4 main(centroid float3 bary : SV_Barycentrics) : SV_Target {
    return float4(bary, 1.0);
// CHECK:      %param_var_bary = OpVariable %_ptr_Function_v3float Function
// CHECK-NEXT:     [[c2:%\d+]] = OpLoad %v3float [[bary]]
// CHECK-NEXT:     OpStore %param_var_bary [[c2]]
}
