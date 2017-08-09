// Run: %dxc -T vs_6_0 -E main

// CHECK:                   OpEntryPoint Vertex %main "main" {{%\d+}} %gl_VertexIndex

// CHECK:                   OpDecorate %gl_VertexIndex BuiltIn VertexIndex

// CHECK: %gl_VertexIndex = OpVariable %_ptr_Input_int Input

int main(int input: SV_VertexID) : A {
    return input;
}
