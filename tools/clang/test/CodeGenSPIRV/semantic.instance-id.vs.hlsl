// Run: %dxc -T vs_6_0 -E main

// CHECK:                     OpEntryPoint Vertex %main "main" [[output:%\d+]] %gl_InstanceIndex

// CHECK:                     OpDecorate %gl_InstanceIndex BuiltIn InstanceIndex
// CHECK:                     OpDecorate [[output]] Location 0

// CHECK:        [[output]] = OpVariable %_ptr_Output_int Output
// CHECK: %gl_InstanceIndex = OpVariable %_ptr_Input_int Input

int main(int input: SV_InstanceID) : SV_InstanceID {
    return input;
}

