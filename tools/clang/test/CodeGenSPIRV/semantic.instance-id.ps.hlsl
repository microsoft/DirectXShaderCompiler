// Run: %dxc -T ps_6_0 -E main

// CHECK:             OpEntryPoint Fragment %main "main" %in_var_SV_InstanceID %out_var_SV_Target

// CHECK:             OpDecorate %in_var_SV_InstanceID Location 0

// CHECK: %in_var_SV_InstanceID = OpVariable %_ptr_Input_int Input

float4 main(int input: SV_InstanceID) : SV_Target {
    return input;
}
