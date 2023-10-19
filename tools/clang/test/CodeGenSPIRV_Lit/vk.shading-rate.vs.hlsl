// RUN: %dxc -T vs_6_4 -E main

void main(out uint rate : SV_ShadingRate) {
// CHECK:   OpCapability FragmentShadingRateKHR
// CHECK:   OpExtension "SPV_KHR_fragment_shading_rate"
// CHECK:   OpDecorate [[r:%\d+]] BuiltIn PrimitiveShadingRateKHR
// CHECK:   [[r:%\d+]] = OpVariable %_ptr_Output_uint Output
    rate = 0;
}
