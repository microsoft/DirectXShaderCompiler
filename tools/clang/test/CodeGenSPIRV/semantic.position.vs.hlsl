// Run: %dxc -T vs_6_0 -E main

// CHECK:                OpEntryPoint Vertex %main "main" %gl_Position [[input:%\d+]]

// CHECK:                OpDecorate %gl_Position BuiltIn Position
// CHECK:                OpDecorate [[input]] Location 0

// CHECK: %gl_Position = OpVariable %_ptr_Output_v4float Output
// CHECK:    [[input]] = OpVariable %_ptr_Input_v4float Input

float4 main(float4 input: SV_Position) : SV_Position {
    return input;
}
