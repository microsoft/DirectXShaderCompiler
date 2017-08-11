// Run: %dxc -T ps_6_0 -E main

// CHECK:              OpEntryPoint Fragment %main "main" %in_var_A %out_var_SV_Target

// CHECK:              OpDecorate %out_var_SV_Target Location 0

// CHECK: %out_var_SV_Target = OpVariable %_ptr_Output_v4float Output

float4 main(float4 input: A) : SV_Target {
    return input;
}
