// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpEntryPoint Vertex %main "main"
// CHECK-SAME: %in_var_SV_ViewportArrayIndex
// CHECK-SMAE: %out_var_SV_ViewportArrayIndex

// CHECK:      OpDecorate %in_var_SV_ViewportArrayIndex Location 0
// CHECK:      OpDecorate %out_var_SV_ViewportArrayIndex Location 0

// CHECK:      %in_var_SV_ViewportArrayIndex = OpVariable %_ptr_Input_uint Input
// CHECK:      %out_var_SV_ViewportArrayIndex = OpVariable %_ptr_Output_uint Output

uint main(uint input: SV_ViewportArrayIndex) : SV_ViewportArrayIndex {
    return input;
}
