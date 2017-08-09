// Run: %dxc -T ps_6_0 -E main

// CHECK:                 OpEntryPoint Fragment %main "main" %gl_FragDepth {{%\d+}}

// CHECK:                 OpDecorate %gl_FragDepth BuiltIn FragDepth

// CHECK: %gl_FragDepth = OpVariable %_ptr_Output_float Output

float main(float input: A) : SV_Depth {
    return input;
}
