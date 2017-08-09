// Run: %dxc -T ps_6_0 -E main

// CHECK:             OpEntryPoint Fragment %main "main" {{%\d+}} [[input:%\d+]]

// CHECK:             OpDecorate [[input]] Location 0

// CHECK: [[input]] = OpVariable %_ptr_Input_int Input

float4 main(int input: SV_InstanceID) : SV_Target {
    return input;
}
