// Run: %dxc -T ps_6_0 -E main

void main() {
    bool r;
    int a, b;
    uint i, j;
    float o, p;

// CHECK:      [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT: %{{\d+}} = OpSLessThan %bool [[a]] [[b]]
    r = a < b;

// CHECK:      [[i:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: %{{\d+}} = OpULessThan %bool [[i]] [[j]]
    r = i < j;

// CHECK:      [[o:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p:%\d+]] = OpLoad %float %p
// CHECK-NEXT: %{{\d+}} = OpFOrdLessThan %bool [[o]] [[p]]
    r = o < p;
}
