// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    bool a, b, c;
    // Plain assign (scalar)
// CHECK:      [[a0:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %bool %b
// CHECK-NEXT: [[or0:%\d+]] = OpLogicalOr %bool [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[or0]]
    c = a || b;

    bool1 i, j, k;
    bool3 o, p, q;
    // Plain assign (vector)
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %bool %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %bool %j
// CHECK-NEXT: [[or1:%\d+]] = OpLogicalOr %bool [[i0]] [[j0]]
// CHECK-NEXT: OpStore %k [[or1]]
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %v3bool %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v3bool %p
// CHECK-NEXT: [[or2:%\d+]] = OpLogicalOr %v3bool [[o0]] [[p0]]
// CHECK-NEXT: OpStore %q [[or2]]
    k = i || j;
    q = o || p;
}
