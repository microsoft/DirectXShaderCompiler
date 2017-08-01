// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpSNegate %int [[a0]]
// CHECK-NEXT: OpStore %b [[b0]]
    b = -a;

    uint c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %uint %c
// CHECK-NEXT: [[d0:%\d+]] = OpSNegate %uint [[c0]]
// CHECK-NEXT: OpStore %d [[d0]]
    d = -c;

    float i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %float %i
// CHECK-NEXT: [[j0:%\d+]] = OpFNegate %float [[i0]]
// CHECK-NEXT: OpStore %j [[j0]]
    j = -i;

    float1 m, n;
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %float %m
// CHECK-NEXT: [[n0:%\d+]] = OpFNegate %float [[m0]]
// CHECK-NEXT: OpStore %n [[n0]]
    n = -m;

    int3 x, y;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3int %x
// CHECK-NEXT: [[y0:%\d+]] = OpSNegate %v3int [[x0]]
// CHECK-NEXT: OpStore %y [[y0]]
    y = -x;
}
