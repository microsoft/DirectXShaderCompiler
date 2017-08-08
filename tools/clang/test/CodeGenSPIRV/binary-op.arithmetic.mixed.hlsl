// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float4 a, b;
    float s;

    int3 c, d;
    int t;

    float1 e, f;
    int1 g, h;

    float2x3 i, j;
    float1x3 k, l;
    float2x1 m, n;
    float1x1 o, p;

    // Use OpVectorTimesScalar for floatN * float
// CHECK:      [[a4:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s0:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul0:%\d+]] = OpVectorTimesScalar %v4float [[a4]] [[s0]]
// CHECK-NEXT: OpStore %b [[mul0]]
    b = a * s;
// CHECK-NEXT: [[a5:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul1:%\d+]] = OpVectorTimesScalar %v4float [[a5]] [[s1]]
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

    // Vector of size 1
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %float %e
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul4:%\d+]] = OpFMul %float [[e0]] [[s2]]
// CHECK-NEXT: OpStore %f [[mul4]]
    f = e * s;
// CHECK-NEXT: [[s3:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[e1:%\d+]] = OpLoad %float %e
// CHECK-NEXT: [[mul5:%\d+]] = OpFMul %float [[s3]] [[e1]]
// CHECK-NEXT: OpStore %f [[mul5]]
    f = s * e;
// CHECK-NEXT: [[g0:%\d+]] = OpLoad %int %g
// CHECK-NEXT: [[t2:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[mul6:%\d+]] = OpIMul %int [[g0]] [[t2]]
// CHECK-NEXT: OpStore %h [[mul6]]
    h = g * t;
// CHECK-NEXT: [[t3:%\d+]] = OpLoad %int %t
// CHECK-NEXT: [[g1:%\d+]] = OpLoad %int %g
// CHECK-NEXT: [[mul7:%\d+]] = OpIMul %int [[t3]] [[g1]]
// CHECK-NEXT: OpStore %h [[mul7]]
    h = t * g;

    // Use OpMatrixTimesScalar for floatMxN * float
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %mat2v3float %i
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul8:%\d+]] = OpMatrixTimesScalar %mat2v3float [[i0]] [[s4]]
// CHECK-NEXT: OpStore %j [[mul8]]
    j = i * s;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %mat2v3float %i
// CHECK-NEXT: [[s5:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul9:%\d+]] = OpMatrixTimesScalar %mat2v3float [[i1]] [[s5]]
// CHECK-NEXT: OpStore %j [[mul9]]
    j = s * i;

    // Use OpVectorTimesScalar for float1xN * float
// CHECK-NEXT: [[k0:%\d+]] = OpLoad %v3float %k
// CHECK-NEXT: [[s6:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul10:%\d+]] = OpVectorTimesScalar %v3float [[k0]] [[s6]]
// CHECK-NEXT: OpStore %l [[mul10]]
    l = k * s;
// CHECK-NEXT: [[k1:%\d+]] = OpLoad %v3float %k
// CHECK-NEXT: [[s7:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul11:%\d+]] = OpVectorTimesScalar %v3float [[k1]] [[s7]]
// CHECK-NEXT: OpStore %l [[mul11]]
    l = s * k;

    // Use OpVectorTimesScalar for floatMx1 * float
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %v2float %m
// CHECK-NEXT: [[s8:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul12:%\d+]] = OpVectorTimesScalar %v2float [[m0]] [[s8]]
// CHECK-NEXT: OpStore %n [[mul12]]
    n = m * s;
// CHECK-NEXT: [[m1:%\d+]] = OpLoad %v2float %m
// CHECK-NEXT: [[s9:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul13:%\d+]] = OpVectorTimesScalar %v2float [[m1]] [[s9]]
// CHECK-NEXT: OpStore %n [[mul13]]
    n = s * m;

    // Matrix of size 1x1
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[s10:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[mul14:%\d+]] = OpFMul %float [[o0]] [[s10]]
// CHECK-NEXT: OpStore %p [[mul14]]
    p = o * s;
// CHECK-NEXT: [[s11:%\d+]] = OpLoad %float %s
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[mul15:%\d+]] = OpFMul %float [[s11]] [[o1]]
// CHECK-NEXT: OpStore %p [[mul15]]
    p = s * o;
}
