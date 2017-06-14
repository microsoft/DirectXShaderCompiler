// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a, b;
    float s;

    int3 c, d;
    int t;

// CHECK:      [[a0:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s0:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v4float [[s0]] [[s0]] [[s0]] [[s0]]
// CHECK-NEXT: [[add0:%\d+]] = OpFAdd %v4float [[a0]] [[cc0]]
// CHECK-NEXT: OpStore %b [[add0]]
    b = a + s;
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v4float [[s1]] [[s1]] [[s1]] [[s1]]
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[add1:%\d+]] = OpFAdd %v4float [[cc1]] [[a1]]
// CHECK-NEXT: OpStore %b [[add1]]
    b = s + a;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v4float [[s2]] [[s2]] [[s2]] [[s2]]
// CHECK-NEXT: [[sub0:%\d+]] = OpFSub %v4float [[a2]] [[cc2]]
// CHECK-NEXT: OpStore %b [[sub0]]
    b = a - s;
// CHECK-NEXT: [[s3:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %v4float [[s3]] [[s3]] [[s3]] [[s3]]
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[sub1:%\d+]] = OpFSub %v4float [[cc3]] [[a3]]
// CHECK-NEXT: OpStore %b [[sub1]]
    b = s - a;

    // Use OpVectorTimesScalar for floatN * float
// CHECK-NEXT: [[a4:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a4]] [[s4]]
// CHECK-NEXT: OpStore %b [[mul0]]
    b = a * s;
// CHECK-NEXT: [[a5:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s5:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul1:%\d+]] = OpVectorTimesScalar %v4float [[a5]] [[s5]]
// CHECK-NEXT: OpStore %b [[mul1]]
    b = s * a;

// CHECK-NEXT: [[a6:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s6:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc6:%\d+]] = OpCompositeConstruct %v4float [[s6]] [[s6]] [[s6]] [[s6]]
// CHECK-NEXT: [[div0:%\d+]] = OpFDiv %v4float [[a6]] [[cc6]]
// CHECK-NEXT: OpStore %b [[div0]]
    b = a / s;
// CHECK-NEXT: [[s7:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc7:%\d+]] = OpCompositeConstruct %v4float [[s7]] [[s7]] [[s7]] [[s7]]
// CHECK-NEXT: [[a7:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[div1:%\d+]] = OpFDiv %v4float [[cc7]] [[a7]]
// CHECK-NEXT: OpStore %b [[div1]]
    b = s / a;

// CHECK-NEXT: [[a8:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s8:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc8:%\d+]] = OpCompositeConstruct %v4float [[s8]] [[s8]] [[s8]] [[s8]]
// CHECK-NEXT: [[mod0:%\d+]] = OpFRem %v4float [[a8]] [[cc8]]
// CHECK-NEXT: OpStore %b [[mod0]]
    b = a % s;
// CHECK-NEXT: [[s9:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc9:%\d+]] = OpCompositeConstruct %v4float [[s9]] [[s9]] [[s9]] [[s9]]
// CHECK-NEXT: [[a9:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[mod1:%\d+]] = OpFRem %v4float [[cc9]] [[a9]]
// CHECK-NEXT: OpStore %b [[mod1]]
    b = s % a;

    // Use normal OpCompositeConstruct and OpIMul for intN * int
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v3int %c
// CHECK-NEXT: [[t0:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v3int [[t0]] [[t0]] [[t0]]
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3int [[c0]] [[cc10]]
// CHECK-NEXT: OpStore %d [[mul2]]
    d = c * t;
// CHECK-NEXT: [[t1:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[cc11:%\d+]] = OpCompositeConstruct %v3int [[t1]] [[t1]] [[t1]]
// CHECK-NEXT: [[c1:%\d+]] = OpLoad %v3int %c
// CHECK-NEXT: [[mul3:%\d+]] = OpIMul %v3int [[cc11]] [[c1]]
// CHECK-NEXT: OpStore %d [[mul3]]
    d = t * c;
}
