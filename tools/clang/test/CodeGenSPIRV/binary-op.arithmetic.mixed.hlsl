// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a, b;
    float s;

    int3 c, d;
    int t;

    // Use OpVectorTimesScalar for floatN * float
// CHECK:      [[a4:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a4]] [[s4]]
// CHECK-NEXT: OpStore %b [[mul0]]
    b = a * s;
// CHECK-NEXT: [[a5:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s5:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul1:%\d+]] = OpVectorTimesScalar %v4float [[a5]] [[s5]]
// CHECK-NEXT: OpStore %b [[mul1]]
    b = s * a;

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
