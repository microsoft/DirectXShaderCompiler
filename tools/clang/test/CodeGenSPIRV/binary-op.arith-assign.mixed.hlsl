// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a;
    float s;

    int3 c;
    int t;

    float1 e;
    int1 g;

    float2x3 i;
    float1x3 k;
    float2x1 m;
    float1x1 o;

    // Use OpVectorTimesScalar for floatN * float
// CHECK:      [[s0:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[a0:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a0]] [[s0]]
// CHECK-NEXT: OpStore %a [[mul0]]
    a *= s;

    // Use normal OpCompositeConstruct and OpIMul for intN * int
// CHECK-NEXT: [[t0:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3int [[t0]] [[t0]] [[t0]]
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v3int %c
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3int [[c0]] [[cc0]]
// CHECK-NEXT: OpStore %c [[mul2]]
    c *= t;

    // Vector of size 1
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %float %e
// CHECK-NEXT: [[mul4:%\d+]] = OpFMul %float [[e0]] [[s2]]
// CHECK-NEXT: OpStore %e [[mul4]]
    e *= s;
// CHECK-NEXT: [[t2:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[g0:%\d+]] = OpLoad %int %g
// CHECK-NEXT: [[mul6:%\d+]] = OpIMul %int [[g0]] [[t2]]
// CHECK-NEXT: OpStore %g [[mul6]]
    g *= t;

    // Use OpMatrixTimesScalar for floatMxN * float
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %mat2v3float %i
// CHECK-NEXT: [[mul8:%\d+]] = OpMatrixTimesScalar %mat2v3float [[i0]] [[s4]]
// CHECK-NEXT: OpStore %i [[mul8]]
    i *= s;

    // Use OpVectorTimesScalar for float1xN * float
    // Sadly, the AST is constructed differently for 'float1xN *= float' cases.
    // So we are not able generate an OpVectorTimesScalar here.
    // TODO: Minor issue. Fix this later maybe.
// CHECK-NEXT: [[s6:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v3float [[s6]] [[s6]] [[s6]]
// CHECK-NEXT: [[k0:%\d+]] = OpLoad %v3float %k
// CHECK-NEXT: [[mul10:%\d+]] = OpFMul %v3float [[k0]] [[cc1]]
// CHECK-NEXT: OpStore %k [[mul10]]
    k *= s;

    // Use OpVectorTimesScalar for floatMx1 * float
// CHECK-NEXT: [[s8:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %v2float %m
// CHECK-NEXT: [[mul12:%\d+]] = OpVectorTimesScalar %v2float [[m0]] [[s8]]
// CHECK-NEXT: OpStore %m [[mul12]]
    m *= s;

    // Matrix of size 1x1
// CHECK-NEXT: [[s10:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[mul14:%\d+]] = OpFMul %float [[o0]] [[s10]]
// CHECK-NEXT: OpStore %o [[mul14]]
    o *= s;
}
