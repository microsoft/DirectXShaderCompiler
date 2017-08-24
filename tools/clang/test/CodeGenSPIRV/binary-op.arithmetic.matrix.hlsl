// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // 1x1
    float1x1 a, b, c;
// CHECK:      [[a0:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[c0:%\d+]] = OpFAdd %float [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[c0]]
    c = a + b;
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[c1:%\d+]] = OpFSub %float [[a1]] [[b1]]
// CHECK-NEXT: OpStore %c [[c1]]
    c = a - b;
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[c2:%\d+]] = OpFMul %float [[a2]] [[b2]]
// CHECK-NEXT: OpStore %c [[c2]]
    c = a * b;
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[c3:%\d+]] = OpFDiv %float [[a3]] [[b3]]
// CHECK-NEXT: OpStore %c [[c3]]
    c = a / b;
// CHECK-NEXT: [[a4:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[c4:%\d+]] = OpFRem %float [[a4]] [[b4]]
// CHECK-NEXT: OpStore %c [[c4]]
    c = a % b;

    // Mx1
    float2x1 h, i, j;
// CHECK-NEXT: [[h0:%\d+]] = OpLoad %v2float %h
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v2float %i
// CHECK-NEXT: [[j0:%\d+]] = OpFAdd %v2float [[h0]] [[i0]]
// CHECK-NEXT: OpStore %j [[j0]]
    j = h + i;
// CHECK-NEXT: [[h1:%\d+]] = OpLoad %v2float %h
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %v2float %i
// CHECK-NEXT: [[j1:%\d+]] = OpFSub %v2float [[h1]] [[i1]]
// CHECK-NEXT: OpStore %j [[j1]]
    j = h - i;
// CHECK-NEXT: [[h2:%\d+]] = OpLoad %v2float %h
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %v2float %i
// CHECK-NEXT: [[j2:%\d+]] = OpFMul %v2float [[h2]] [[i2]]
// CHECK-NEXT: OpStore %j [[j2]]
    j = h * i;
// CHECK-NEXT: [[h3:%\d+]] = OpLoad %v2float %h
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %v2float %i
// CHECK-NEXT: [[j3:%\d+]] = OpFDiv %v2float [[h3]] [[i3]]
// CHECK-NEXT: OpStore %j [[j3]]
    j = h / i;
// CHECK-NEXT: [[h4:%\d+]] = OpLoad %v2float %h
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %v2float %i
// CHECK-NEXT: [[j4:%\d+]] = OpFRem %v2float [[h4]] [[i4]]
// CHECK-NEXT: OpStore %j [[j4]]
    j = h % i;

    // 1xN
    float1x3 o, p, q;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v3float %p
// CHECK-NEXT: [[q0:%\d+]] = OpFAdd %v3float [[o0]] [[p0]]
// CHECK-NEXT: OpStore %q [[q0]]
    q = o + p;
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %v3float %p
// CHECK-NEXT: [[q1:%\d+]] = OpFSub %v3float [[o1]] [[p1]]
// CHECK-NEXT: OpStore %q [[q1]]
    q = o - p;
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %v3float %p
// CHECK-NEXT: [[q2:%\d+]] = OpFMul %v3float [[o2]] [[p2]]
// CHECK-NEXT: OpStore %q [[q2]]
    q = o * p;
// CHECK-NEXT: [[o3:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT: [[p3:%\d+]] = OpLoad %v3float %p
// CHECK-NEXT: [[q3:%\d+]] = OpFDiv %v3float [[o3]] [[p3]]
// CHECK-NEXT: OpStore %q [[q3]]
    q = o / p;
// CHECK-NEXT: [[o4:%\d+]] = OpLoad %v3float %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %v3float %p
// CHECK-NEXT: [[q4:%\d+]] = OpFRem %v3float [[o4]] [[p4]]
// CHECK-NEXT: OpStore %q [[q4]]
    q = o % p;

    // MxN
    float2x3 r, s, t;
// CHECK-NEXT: [[r0:%\d+]] = OpLoad %mat2v3float %r
// CHECK-NEXT: [[s0:%\d+]] = OpLoad %mat2v3float %s
// CHECK-NEXT: [[r0v0:%\d+]] = OpCompositeExtract %v3float [[r0]] 0
// CHECK-NEXT: [[s0v0:%\d+]] = OpCompositeExtract %v3float [[s0]] 0
// CHECK-NEXT: [[t0v0:%\d+]] = OpFAdd %v3float [[r0v0]] [[s0v0]]
// CHECK-NEXT: [[r0v1:%\d+]] = OpCompositeExtract %v3float [[r0]] 1
// CHECK-NEXT: [[s0v1:%\d+]] = OpCompositeExtract %v3float [[s0]] 1
// CHECK-NEXT: [[t0v1:%\d+]] = OpFAdd %v3float [[r0v1]] [[s0v1]]
// CHECK-NEXT: [[t0:%\d+]] = OpCompositeConstruct %mat2v3float [[t0v0]] [[t0v1]]
// CHECK-NEXT: OpStore %t [[t0]]
    t = r + s;
// CHECK-NEXT: [[r1:%\d+]] = OpLoad %mat2v3float %r
// CHECK-NEXT: [[s1:%\d+]] = OpLoad %mat2v3float %s
// CHECK-NEXT: [[r1v0:%\d+]] = OpCompositeExtract %v3float [[r1]] 0
// CHECK-NEXT: [[s1v0:%\d+]] = OpCompositeExtract %v3float [[s1]] 0
// CHECK-NEXT: [[t1v0:%\d+]] = OpFSub %v3float [[r1v0]] [[s1v0]]
// CHECK-NEXT: [[r1v1:%\d+]] = OpCompositeExtract %v3float [[r1]] 1
// CHECK-NEXT: [[s1v1:%\d+]] = OpCompositeExtract %v3float [[s1]] 1
// CHECK-NEXT: [[t1v1:%\d+]] = OpFSub %v3float [[r1v1]] [[s1v1]]
// CHECK-NEXT: [[t1:%\d+]] = OpCompositeConstruct %mat2v3float [[t1v0]] [[t1v1]]
// CHECK-NEXT: OpStore %t [[t1]]
    t = r - s;
// CHECK-NEXT: [[r2:%\d+]] = OpLoad %mat2v3float %r
// CHECK-NEXT: [[s2:%\d+]] = OpLoad %mat2v3float %s
// CHECK-NEXT: [[r2v0:%\d+]] = OpCompositeExtract %v3float [[r2]] 0
// CHECK-NEXT: [[s2v0:%\d+]] = OpCompositeExtract %v3float [[s2]] 0
// CHECK-NEXT: [[t2v0:%\d+]] = OpFMul %v3float [[r2v0]] [[s2v0]]
// CHECK-NEXT: [[r2v1:%\d+]] = OpCompositeExtract %v3float [[r2]] 1
// CHECK-NEXT: [[s2v1:%\d+]] = OpCompositeExtract %v3float [[s2]] 1
// CHECK-NEXT: [[t2v1:%\d+]] = OpFMul %v3float [[r2v1]] [[s2v1]]
// CHECK-NEXT: [[t2:%\d+]] = OpCompositeConstruct %mat2v3float [[t2v0]] [[t2v1]]
// CHECK-NEXT: OpStore %t [[t2]]
    t = r * s;
// CHECK-NEXT: [[r3:%\d+]] = OpLoad %mat2v3float %r
// CHECK-NEXT: [[s3:%\d+]] = OpLoad %mat2v3float %s
// CHECK-NEXT: [[r3v0:%\d+]] = OpCompositeExtract %v3float [[r3]] 0
// CHECK-NEXT: [[s3v0:%\d+]] = OpCompositeExtract %v3float [[s3]] 0
// CHECK-NEXT: [[t3v0:%\d+]] = OpFDiv %v3float [[r3v0]] [[s3v0]]
// CHECK-NEXT: [[r3v1:%\d+]] = OpCompositeExtract %v3float [[r3]] 1
// CHECK-NEXT: [[s3v1:%\d+]] = OpCompositeExtract %v3float [[s3]] 1
// CHECK-NEXT: [[t3v1:%\d+]] = OpFDiv %v3float [[r3v1]] [[s3v1]]
// CHECK-NEXT: [[t3:%\d+]] = OpCompositeConstruct %mat2v3float [[t3v0]] [[t3v1]]
// CHECK-NEXT: OpStore %t [[t3]]
    t = r / s;
// CHECK-NEXT: [[r4:%\d+]] = OpLoad %mat2v3float %r
// CHECK-NEXT: [[s4:%\d+]] = OpLoad %mat2v3float %s
// CHECK-NEXT: [[r4v0:%\d+]] = OpCompositeExtract %v3float [[r4]] 0
// CHECK-NEXT: [[s4v0:%\d+]] = OpCompositeExtract %v3float [[s4]] 0
// CHECK-NEXT: [[t4v0:%\d+]] = OpFRem %v3float [[r4v0]] [[s4v0]]
// CHECK-NEXT: [[r4v1:%\d+]] = OpCompositeExtract %v3float [[r4]] 1
// CHECK-NEXT: [[s4v1:%\d+]] = OpCompositeExtract %v3float [[s4]] 1
// CHECK-NEXT: [[t4v1:%\d+]] = OpFRem %v3float [[r4v1]] [[s4v1]]
// CHECK-NEXT: [[t4:%\d+]] = OpCompositeConstruct %mat2v3float [[t4v0]] [[t4v1]]
// CHECK-NEXT: OpStore %t [[t4]]
    t = r % s;
}
