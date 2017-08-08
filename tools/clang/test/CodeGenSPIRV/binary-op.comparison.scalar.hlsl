// Run: %dxc -T ps_6_0 -E main

void main() {
    bool r;
    int a, b;
    uint i, j;
    float o, p;
    bool x, y;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSLessThan %bool [[a0]] [[b0]]
    r = a < b;
// CHECK:      [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSLessThanEqual %bool [[a1]] [[b1]]
    r = a <= b;
// CHECK:      [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSGreaterThan %bool [[a2]] [[b2]]
    r = a > b;
// CHECK:      [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpSGreaterThanEqual %bool [[a3]] [[b3]]
    r = a >= b;
// CHECK:      [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpIEqual %bool [[a4]] [[b4]]
    r = a == b;
// CHECK:      [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: {{%\d+}} = OpINotEqual %bool [[a5]] [[b5]]
    r = a != b;

// CHECK:      [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpULessThan %bool [[i0]] [[j0]]
    r = i < j;
// CHECK:      [[i1:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpULessThanEqual %bool [[i1]] [[j1]]
    r = i <= j;
// CHECK:      [[i2:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpUGreaterThan %bool [[i2]] [[j2]]
    r = i > j;
// CHECK:      [[i3:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpUGreaterThanEqual %bool [[i3]] [[j3]]
    r = i >= j;
// CHECK:      [[i4:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpIEqual %bool [[i4]] [[j4]]
    r = i == j;
// CHECK:      [[i5:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: {{%\d+}} = OpINotEqual %bool [[i5]] [[j5]]
    r = i != j;

// CHECK:      [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdLessThan %bool [[o0]] [[p0]]
    r = o < p;
// CHECK:      [[o1:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdLessThanEqual %bool [[o1]] [[p1]]
    r = o <= p;
// CHECK:      [[o2:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdGreaterThan %bool [[o2]] [[p2]]
    r = o > p;
// CHECK:      [[o3:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p3:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdGreaterThanEqual %bool [[o3]] [[p3]]
    r = o >= p;
// CHECK:      [[o4:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdEqual %bool [[o4]] [[p4]]
    r = o == p;
// CHECK:      [[o5:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p5:%\d+]] = OpLoad %float %p
// CHECK-NEXT: {{%\d+}} = OpFOrdNotEqual %bool [[o5]] [[p5]]
    r = o != p;

// CHECK:      [[x0:%\d+]] = OpLoad %bool %x
// CHECK-NEXT: [[y0:%\d+]] = OpLoad %bool %y
// CHECK-NEXT: {{%\d+}} = OpLogicalEqual %bool [[x0]] [[y0]]
    r = x == y;
// CHECK:      [[x1:%\d+]] = OpLoad %bool %x
// CHECK-NEXT: [[y1:%\d+]] = OpLoad %bool %y
// CHECK-NEXT: {{%\d+}} = OpLogicalNotEqual %bool [[x1]] [[y1]]
    r = x != y;
}
