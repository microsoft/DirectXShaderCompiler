// RUN: %dxc -T ps_6_0 -E main -HV 2021
// RUN: %dxc -T ps_6_0 -E main -HV 2018

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    bool a, b, c;
    // Plain assign (scalar)
// CHECK:      [[a0:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %bool %b
// CHECK-NEXT: [[and0:%\d+]] = OpLogicalAnd %bool [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[and0]]
    c = and(a, b);

    bool1 i, j, k;
    bool3 o, p, q;
    // Plain assign (vector)
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %bool %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %bool %j
// CHECK-NEXT: [[and1:%\d+]] = OpLogicalAnd %bool [[i0]] [[j0]]
// CHECK-NEXT: OpStore %k [[and1]]
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %v3bool %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v3bool %p
// CHECK-NEXT: [[and2:%\d+]] = OpLogicalAnd %v3bool [[o0]] [[p0]]
// CHECK-NEXT: OpStore %q [[and2]]
    k = and(i, j);
    q = and(o, p);

// The result of '&&' could be 'const bool'. In such cases, make sure
// the result type is correct.
// CHECK:        [[a1:%\d+]] = OpLoad %bool %a
// CHECK-NEXT:   [[b1:%\d+]] = OpLoad %bool %b
// CHECK-NEXT: [[and3:%\d+]] = OpLogicalAnd %bool [[a1]] [[b1]]
// CHECK-NEXT:      {{%\d+}} = OpCompositeConstruct %v2bool [[and3]] %true
    bool2 t = bool2(and(a, b), true);
}
