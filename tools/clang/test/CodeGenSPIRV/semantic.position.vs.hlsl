// Run: %dxc -T vs_6_0 -E main

// CHECK:                OpEntryPoint Vertex %main "main" %in_var_SV_Position %gl_Position

// CHECK:                OpDecorate %gl_Position BuiltIn Position
// CHECK:                OpDecorate %in_var_SV_Position Location 0

// CHECK: %in_var_SV_Position = OpVariable %_ptr_Input_v4float Input
// CHECK: %gl_Position = OpVariable %_ptr_Output_v4float Output

float4 main(float4 input: SV_Position) : SV_Position {
    return input;
}
