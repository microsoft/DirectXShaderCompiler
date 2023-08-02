// RUN: %dxc -T ps_6_4 -E main

float4 main(uint rate : SV_ShadingRate) : SV_TARGET {
// CHECK:   OpCapability FragmentShadingRateKHR
// CHECK:   OpExtension "SPV_KHR_fragment_shading_rate"
// CHECK:   OpDecorate [[r:%\d+]] BuiltIn ShadingRateKHR
// CHECK:   [[r:%\d+]] = OpVariable %_ptr_Input_uint Input
  return float4(rate, 0, 0, 0);
}
