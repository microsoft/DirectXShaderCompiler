// Run: %dxc -T ps_6_0 -E main

// CHECK:              OpEntryPoint Fragment %main "main" [[target:%\d+]] {{%\d+}}

// CHECK:              OpDecorate [[target]] Location 0

// CHECK: [[target]] = OpVariable %_ptr_Output_v4float Output

float4 main(float4 input: A) : SV_Target {
    return input;
}
