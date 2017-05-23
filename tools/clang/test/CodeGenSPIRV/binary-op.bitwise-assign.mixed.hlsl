// Run: %dxc -T vs_6_0 -E main

// TODO: <scalar> <op>= <vector>. This is allowed in HLSL. It will incur an
// implicit truncation which only stores the first element into the <scalar>.

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int3 a;
    int s;

// CHECK:      [[s0:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3int [[s0]] [[s0]] [[s0]]
// CHECK-NEXT: [[a0:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[and0:%\d+]] = OpBitwiseAnd %v3int [[a0]] [[cc0]]
// CHECK-NEXT: OpStore %a [[and0]]
    a &= s;
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v3int [[s1]] [[s1]] [[s1]]
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[or0:%\d+]] = OpBitwiseOr %v3int [[a1]] [[cc1]]
// CHECK-NEXT: OpStore %a [[or0]]
    a |= s;
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v3int [[s2]] [[s2]] [[s2]]
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[xor0:%\d+]] = OpBitwiseXor %v3int [[a2]] [[cc2]]
// CHECK-NEXT: OpStore %a [[xor0]]
    a ^= s;
// CHECK-NEXT: [[s3:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %v3int [[s3]] [[s3]] [[s3]]
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[shl0:%\d+]] = OpShiftLeftLogical %v3int [[a3]] [[cc3]]
// CHECK-NEXT: OpStore %a [[shl0]]
    a <<= s;
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc4:%\d+]] = OpCompositeConstruct %v3int [[s4]] [[s4]] [[s4]]
// CHECK-NEXT: [[a4:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[shr0:%\d+]] = OpShiftRightArithmetic %v3int [[a4]] [[cc4]]
// CHECK-NEXT: OpStore %a [[shr0]]
    a >>= s;
}