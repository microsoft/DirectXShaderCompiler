// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    bool b0;
    int m, n, o;
    // Plain assign (scalar)
// CHECK:      [[b0:%\d+]] = OpLoad %bool %b0
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %int %m
// CHECK-NEXT: [[n0:%\d+]] = OpLoad %int %n
// CHECK-NEXT: [[s0:%\d+]] = OpSelect %int [[b0]] [[m0]] [[n0]]
// CHECK-NEXT: OpStore %o [[s0]]
    o = b0 ? m : n;


    bool1 b1;
    bool3 b3;
    uint1 p, q, r;
    float3 x, y, z;
    // Plain assign (vector)
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %bool %b1
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %uint %p
// CHECK-NEXT: [[q0:%\d+]] = OpLoad %uint %q
// CHECK-NEXT: [[s1:%\d+]] = OpSelect %uint [[b1]] [[p0]] [[q0]]
// CHECK-NEXT: OpStore %r [[s1]]
    r = b1 ? p : q;
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %v3bool %b3
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: [[y0:%\d+]] = OpLoad %v3float %y
// CHECK-NEXT: [[s2:%\d+]] = OpSelect %v3float [[b3]] [[x0]] [[y0]]
// CHECK-NEXT: OpStore %z [[s2]]
    z = b3 ? x : y;
}
