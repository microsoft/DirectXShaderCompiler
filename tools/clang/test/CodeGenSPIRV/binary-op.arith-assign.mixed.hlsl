// Run: %dxc -T vs_6_0 -E main

// TODO: matrix *= scalar

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a;
    float s;

    int3 c;
    int t;

    // Use OpVectorTimesScalar for floatN * float
// CHECK:      [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[a4:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a4]] [[s4]]
// CHECK-NEXT: OpStore %a [[mul0]]
    a *= s;

    // Use normal OpCompositeConstruct and OpIMul for intN * int
// CHECK-NEXT: [[t0:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[cc10:%\d+]] = OpCompositeConstruct %v3int [[t0]] [[t0]] [[t0]]
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v3int %c
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3int [[c0]] [[cc10]]
// CHECK-NEXT: OpStore %c [[mul2]]
    c *= t;
}
