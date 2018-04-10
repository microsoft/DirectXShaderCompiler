// Run: %dxc -T ps_6_0 -E main

// CHECK:      OpCapability FragmentFullyCoveredEXT
// CHECK:      OpExtension "SPV_EXT_fragment_fully_covered"

// CHECK:      OpEntryPoint Fragment
// CHECK-SAME: [[coverage:%\d+]]
// CHECK:      [[coverage]] = OpVariable %_ptr_Input_bool Input


float4 main(uint inCov : SV_InnerCoverage) : SV_Target {
// CHECK:      [[boolv:%\d+]] = OpLoad %bool [[coverage]]
// CHECK-NEXT:  [[intv:%\d+]] = OpSelect %uint [[boolv]] %uint_1 %uint_0
// CHECK-NEXT:                  OpStore %param_var_inCov [[intv]]
    return 1.0;
}
