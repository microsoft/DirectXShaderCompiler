// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int3 a, b;
    int s;

// CHECK:      [[a0:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[s0:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3int [[s0]] [[s0]] [[s0]]
// CHECK-NEXT: [[b0:%\d+]] = OpBitwiseAnd %v3int [[a0]] [[cc0]]
// CHECK-NEXT: OpStore %b [[b0]]
    b = a & s;
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v3int [[s1]] [[s1]] [[s1]]
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[b1:%\d+]] = OpBitwiseAnd %v3int [[cc1]] [[a1]]
// CHECK-NEXT: OpStore %b [[b1]]
    b = s & a;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v3int [[s2]] [[s2]] [[s2]]
// CHECK-NEXT: [[b2:%\d+]] = OpBitwiseOr %v3int [[a2]] [[cc2]]
// CHECK-NEXT: OpStore %b [[b2]]
    b = a | s;
// CHECK-NEXT: [[s3:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %v3int [[s3]] [[s3]] [[s3]]
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[b3:%\d+]] = OpBitwiseOr %v3int [[cc3]] [[a3]]
// CHECK-NEXT: OpStore %b [[b3]]
    b = s | a;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc4:%\d+]] = OpCompositeConstruct %v3int [[s4]] [[s4]] [[s4]]
// CHECK-NEXT: [[b4:%\d+]] = OpBitwiseXor %v3int [[a4]] [[cc4]]
// CHECK-NEXT: OpStore %b [[b4]]
    b = a ^ s;
// CHECK-NEXT: [[s5:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc5:%\d+]] = OpCompositeConstruct %v3int [[s5]] [[s5]] [[s5]]
// CHECK-NEXT: [[a5:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[b5:%\d+]] = OpBitwiseXor %v3int [[cc5]] [[a5]]
// CHECK-NEXT: OpStore %b [[b5]]
    b = s ^ a;

// CHECK-NEXT: [[a6:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[s6:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc6:%\d+]] = OpCompositeConstruct %v3int [[s6]] [[s6]] [[s6]]
// CHECK-NEXT: [[b6:%\d+]] = OpShiftLeftLogical %v3int [[a6]] [[cc6]]
// CHECK-NEXT: OpStore %b [[b6]]
    b = a << s;
// CHECK-NEXT: [[s7:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc7:%\d+]] = OpCompositeConstruct %v3int [[s7]] [[s7]] [[s7]]
// CHECK-NEXT: [[a7:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[b7:%\d+]] = OpShiftLeftLogical %v3int [[cc7]] [[a7]]
// CHECK-NEXT: OpStore %b [[b7]]
    b = s << a;

// CHECK-NEXT: [[a8:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[s8:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc8:%\d+]] = OpCompositeConstruct %v3int [[s8]] [[s8]] [[s8]]
// CHECK-NEXT: [[b8:%\d+]] = OpShiftRightArithmetic %v3int [[a8]] [[cc8]]
// CHECK-NEXT: OpStore %b [[b8]]
    b = a >> s;
// CHECK-NEXT: [[s9:%\d+]] = OpLoad %int %s
// CHECK-NEXT: [[cc9:%\d+]] = OpCompositeConstruct %v3int [[s9]] [[s9]] [[s9]]
// CHECK-NEXT: [[a9:%\d+]] = OpLoad %v3int %a
// CHECK-NEXT: [[b9:%\d+]] = OpShiftRightArithmetic %v3int [[cc9]] [[a9]]
// CHECK-NEXT: OpStore %b [[b9]]
    b = s >> a;
}
