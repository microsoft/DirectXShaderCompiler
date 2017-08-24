// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int1 a, b;
    uint2 i, j;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[and0:%\d+]] = OpBitwiseAnd %int [[b0]] [[a0]]
// CHECK-NEXT: OpStore %b [[and0]]
    b &= a;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v2uint %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %v2uint %j
// CHECK-NEXT: [[and1:%\d+]] = OpBitwiseAnd %v2uint [[j0]] [[i0]]
// CHECK-NEXT: OpStore %j [[and1]]
    j &= i;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[or0:%\d+]] = OpBitwiseOr %int [[b1]] [[a1]]
// CHECK-NEXT: OpStore %b [[or0]]
    b |= a;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %v2uint %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %v2uint %j
// CHECK-NEXT: [[or1:%\d+]] = OpBitwiseOr %v2uint [[j1]] [[i1]]
// CHECK-NEXT: OpStore %j [[or1]]
    j |= i;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[xor0:%\d+]] = OpBitwiseXor %int [[b2]] [[a2]]
// CHECK-NEXT: OpStore %b [[xor0]]
    b ^= a;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %v2uint %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %v2uint %j
// CHECK-NEXT: [[xor1:%\d+]] = OpBitwiseXor %v2uint [[j2]] [[i2]]
// CHECK-NEXT: OpStore %j [[xor1]]
    j ^= i;

// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[shl0:%\d+]] = OpShiftLeftLogical %int [[b3]] [[a3]]
// CHECK-NEXT: OpStore %b [[shl0]]
    b <<= a;
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %v2uint %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %v2uint %j
// CHECK-NEXT: [[shl1:%\d+]] = OpShiftLeftLogical %v2uint [[j3]] [[i3]]
// CHECK-NEXT: OpStore %j [[shl1]]
    j <<= i;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[shr0:%\d+]] = OpShiftRightArithmetic %int [[b4]] [[a4]]
// CHECK-NEXT: OpStore %b [[shr0]]
    b >>= a;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %v2uint %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %v2uint %j
// CHECK-NEXT: [[shr1:%\d+]] = OpShiftRightLogical %v2uint [[j4]] [[i4]]
// CHECK-NEXT: OpStore %j [[shr1]]
    j >>= i;
}
