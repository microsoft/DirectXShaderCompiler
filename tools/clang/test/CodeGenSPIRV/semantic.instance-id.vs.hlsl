// Run: %dxc -T vs_6_0 -E main

// CHECK:                     OpEntryPoint Vertex %main "main" %gl_InstanceIndex %out_var_SV_InstanceID

// CHECK:                     OpDecorate %gl_InstanceIndex BuiltIn InstanceIndex
// CHECK:                     OpDecorate %out_var_SV_InstanceID Location 0

// CHECK: %gl_InstanceIndex = OpVariable %_ptr_Input_int Input
// CHECK: %out_var_SV_InstanceID = OpVariable %_ptr_Output_int Output

int main(int input: SV_InstanceID) : SV_InstanceID {
    return input;
}

