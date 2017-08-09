// Run: %dxc -T vs_6_0 -E main

// CHECK: OpEntryPoint Vertex %main "main" [[out:%\d+]] [[a:%\d+]] [[b:%\d+]] [[c:%\d+]]

// CHECK: OpDecorate [[out]] Location 0
// CHECK: OpDecorate [[a]] Location 0
// CHECK: OpDecorate [[b]] Location 1
// CHECK: OpDecorate [[c]] Location 2

// CHECK: [[out]] = OpVariable %_ptr_Output_float Output
// CHECK: [[a]] = OpVariable %_ptr_Input_v4float Input
// CHECK: [[b]] = OpVariable %_ptr_Input_int Input
// CHECK: [[c]] = OpVariable %_ptr_Input_mat2v3float Input

float main(float4 a: AAA, int b: B, float2x3 c: CC) : DDDD {
    return 1.0;
}
