// Run: %dxc -T ps_6_1 -E main

// CHECK:      OpCapability MultiView
// CHECK:      OpExtension "SPV_KHR_multiview"

// CHECK:      OpEntryPoint Fragment
// CHECK-SAME: [[viewindex:%\d+]]

// CHECK:      OpDecorate [[viewindex]] BuiltIn ViewIndex

// CHECK:      [[viewindex]] = OpVariable %_ptr_Input_uint Input

float4 main(uint viewid: SV_ViewID) : SV_Target {
    return viewid;
}
