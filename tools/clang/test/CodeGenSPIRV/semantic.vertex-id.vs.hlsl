// Run: %dxc -T vs_6_0 -E main

// CHECK:                   OpEntryPoint Vertex %main "main" %gl_VertexIndex %out_var_A

// CHECK:                   OpDecorate %gl_VertexIndex BuiltIn VertexIndex

// CHECK: %gl_VertexIndex = OpVariable %_ptr_Input_int Input

int main(int input: SV_VertexID) : A {
    return input;
}
