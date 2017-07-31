// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    bool  r1;
    bool2 r2;
    bool3 r3;
    bool4 r4;

    int1  a, b;
    int2   i, j;
    uint3  m, n;
    float4 o, p;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSLessThan %bool [[a0]] [[b0]]
    r1 = a < b;
// CHECK:      [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSLessThanEqual %bool [[a1]] [[b1]]
    r1 = a <= b;
// CHECK:      [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSGreaterThan %bool [[a2]] [[b2]]
    r1 = a > b;
// CHECK:      [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSGreaterThanEqual %bool [[a3]] [[b3]]
    r1 = a >= b;
// CHECK:      [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpIEqual %bool [[a4]] [[b4]]
    r1 = a == b;
// CHECK:      [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpINotEqual %bool [[a5]] [[b5]]
    r1 = a != b;

// CHECK:      [[i0:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpSLessThan %v2bool [[i0]] [[j0]]
    r2 = i < j;
// CHECK:      [[i1:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpSLessThanEqual %v2bool [[i1]] [[j1]]
    r2 = i <= j;
// CHECK:      [[i2:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpSGreaterThan %v2bool [[i2]] [[j2]]
    r2 = i > j;
// CHECK:      [[i3:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpSGreaterThanEqual %v2bool [[i3]] [[j3]]
    r2 = i >= j;
// CHECK:      [[i4:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpIEqual %v2bool [[i4]] [[j4]]
    r2 = i == j;
// CHECK:      [[i5:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: {{%\d+}} = OpINotEqual %v2bool [[i5]] [[j5]]
    r2 = i != j;

// CHECK:      [[m0:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n0:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpULessThan %v3bool [[m0]] [[n0]]
    r3 = m < n;
// CHECK:      [[m1:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n1:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpULessThanEqual %v3bool [[m1]] [[n1]]
    r3 = m <= n;
// CHECK:      [[m2:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n2:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpUGreaterThan %v3bool [[m2]] [[n2]]
    r3 = m > n;
// CHECK:      [[m3:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n3:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpUGreaterThanEqual %v3bool [[m3]] [[n3]]
    r3 = m >= n;
// CHECK:      [[m4:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n4:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpIEqual %v3bool [[m4]] [[n4]]
    r3 = m == n;
// CHECK:      [[m5:%\d+]] = OpLoad %v3uint %m
// CHECK-NEXT: [[n5:%\d+]] = OpLoad %v3uint %n
// CHECK-NEXT: {{%\d+}} = OpINotEqual %v3bool [[m5]] [[n5]]
    r3 = m != n;

// CHECK:      [[o0:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdLessThan %v4bool [[o0]] [[p0]]
    r4 = o < p;
// CHECK:      [[o1:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdLessThanEqual %v4bool [[o1]] [[p1]]
    r4 = o <= p;
// CHECK:      [[o2:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdGreaterThan %v4bool [[o2]] [[p2]]
    r4 = o > p;
// CHECK:      [[o3:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p3:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdGreaterThanEqual %v4bool [[o3]] [[p3]]
    r4 = o >= p;
// CHECK:      [[o4:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdEqual %v4bool [[o4]] [[p4]]
    r4 = o == p;
// CHECK:      [[o5:%\d+]] = OpLoad %v4float %o
// CHECK-NEXT: [[p5:%\d+]] = OpLoad %v4float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdNotEqual %v4bool [[o5]] [[p5]]
    r4 = o != p;
}
