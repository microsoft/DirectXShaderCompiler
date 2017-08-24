// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    bool a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[b0:%\d+]] = OpLogicalNot %bool [[a0]]
// CHECK-NEXT: OpStore %b [[b0]]
    b = !a;

    bool1 c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: [[d0:%\d+]] = OpLogicalNot %bool [[c0]]
// CHECK-NEXT: OpStore %d [[d0]]
    d = !c;

    bool2 i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v2bool %i
// CHECK-NEXT: [[j0:%\d+]] = OpLogicalNot %v2bool [[i0]]
// CHECK-NEXT: OpStore %j [[j0]]
    j = !i;
}
