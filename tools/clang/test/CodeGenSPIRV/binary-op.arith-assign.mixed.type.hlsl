// Run: %dxc -T vs_6_0 -E main

void main() {
    uint uVal;
    bool bVal;

    float fVal;
    int iVal;

    // No conversion of lhs
// CHECK:      [[b_bool:%\d+]] = OpLoad %bool %bVal
// CHECK-NEXT: [[b_uint:%\d+]] = OpSelect %uint [[b_bool]] %uint_1 %uint_0
// CHECK-NEXT: [[u_uint:%\d+]] = OpLoad %uint %uVal
// CHECK-NEXT:    [[add:%\d+]] = OpIAdd %uint [[u_uint]] [[b_uint]]
// CHECK-NEXT:                   OpStore %uVal [[add]]
    uVal += bVal;

    // Convert lhs to the type of rhs, do computation, and then convert back
// CHECK:        [[f_float:%\d+]] = OpLoad %float %fVal
// CHECK-NEXT:     [[i_int:%\d+]] = OpLoad %int %iVal
// CHECK-NEXT:   [[i_float:%\d+]] = OpConvertSToF %float [[i_int]]
// CHECK-NEXT: [[mul_float:%\d+]] = OpFMul %float [[i_float]] [[f_float]]
// CHECK-NEXT:   [[mul_int:%\d+]] = OpConvertFToS %int [[mul_float]]
// CHECK-NEXT:                      OpStore %iVal [[mul_int]]
    iVal *= fVal;
}
