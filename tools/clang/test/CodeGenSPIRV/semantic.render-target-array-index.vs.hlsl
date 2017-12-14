// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpEntryPoint Vertex %main "main"
// CHECK-SAME: %in_var_SV_RenderTargetArrayIndex
// CHECK-SMAE: %out_var_SV_RenderTargetArrayIndex

// CHECK:      OpDecorate %in_var_SV_RenderTargetArrayIndex Location 0
// CHECK:      OpDecorate %out_var_SV_RenderTargetArrayIndex Location 0

// CHECK:      %in_var_SV_RenderTargetArrayIndex = OpVariable %_ptr_Input_uint Input
// CHECK:      %out_var_SV_RenderTargetArrayIndex = OpVariable %_ptr_Output_uint Output

uint main(uint input: SV_RenderTargetArrayIndex) : SV_RenderTargetArrayIndex {
    return input;
}
