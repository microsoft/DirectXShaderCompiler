// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a;
    float s;

    int3 c;
    int t;

// CHECK:      [[s0:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v4float [[s0]] [[s0]] [[s0]] [[s0]]
// CHECK-NEXT: [[a0:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[add0:%\d+]] = OpFAdd %v4float [[a0]] [[cc0]]
// CHECK-NEXT: OpStore %a [[add0]]
    a += s;

// CHECK-NEXT: [[s2:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v4float [[s2]] [[s2]] [[s2]] [[s2]]
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[sub0:%\d+]] = OpFSub %v4float [[a2]] [[cc2]]
// CHECK-NEXT: OpStore %a [[sub0]]
    a -= s;

    // Use OpVectorTimesScalar for floatN * float
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[a4:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a4]] [[s4]]
// CHECK-NEXT: OpStore %a [[mul0]]
    a *= s;

// CHECK-NEXT: [[s6:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc6:%\d+]] = OpCompositeConstruct %v4float [[s6]] [[s6]] [[s6]] [[s6]]
// CHECK-NEXT: [[a6:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[div0:%\d+]] = OpFDiv %v4float [[a6]] [[cc6]]
// CHECK-NEXT: OpStore %a [[div0]]
    a /= s;

// CHECK-NEXT: [[s8:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc8:%\d+]] = OpCompositeConstruct %v4float [[s8]] [[s8]] [[s8]] [[s8]]
// CHECK-NEXT: [[a8:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[mod0:%\d+]] = OpFRem %v4float [[a8]] [[cc8]]
// CHECK-NEXT: OpStore %a [[mod0]]
    a %= s;

    // Use normal OpCompositeConstruct and OpIMul for intN * int
// CHECK-NEXT: [[t0:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v3int [[t0]] [[t0]] [[t0]]
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v3int %c
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3int [[c0]] [[cc10]]
// CHECK-NEXT: OpStore %c [[mul2]]
    c *= t;
}
