// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int1 a, b, c;
    uint3 i, j, k;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c0:%\d+]] = OpBitwiseAnd %int [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[c0]]
    c = a & b;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %v3uint %j
// CHECK-NEXT: [[k0:%\d+]] = OpBitwiseAnd %v3uint [[i0]] [[j0]]
// CHECK-NEXT: OpStore %k [[k0]]
    k = i & j;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c1:%\d+]] = OpBitwiseOr %int [[a1]] [[b1]]
// CHECK-NEXT: OpStore %c [[c1]]
    c = a | b;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %v3uint %j
// CHECK-NEXT: [[k1:%\d+]] = OpBitwiseOr %v3uint [[i1]] [[j1]]
// CHECK-NEXT: OpStore %k [[k1]]
    k = i | j;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c2:%\d+]] = OpBitwiseXor %int [[a2]] [[b2]]
// CHECK-NEXT: OpStore %c [[c2]]
    c = a ^ b;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %v3uint %j
// CHECK-NEXT: [[k2:%\d+]] = OpBitwiseXor %v3uint [[i2]] [[j2]]
// CHECK-NEXT: OpStore %k [[k2]]
    k = i ^ j;

// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c3:%\d+]] = OpShiftLeftLogical %int [[a3]] [[b3]]
// CHECK-NEXT: OpStore %c [[c3]]
    c = a << b;
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %v3uint %j
// CHECK-NEXT: [[k3:%\d+]] = OpShiftLeftLogical %v3uint [[i3]] [[j3]]
// CHECK-NEXT: OpStore %k [[k3]]
    k = i << j;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c4:%\d+]] = OpShiftRightArithmetic %int [[a4]] [[b4]]
// CHECK-NEXT: OpStore %c [[c4]]
    c = a >> b;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %v3uint %j
// CHECK-NEXT: [[k4:%\d+]] = OpShiftRightLogical %v3uint [[i4]] [[j4]]
// CHECK-NEXT: OpStore %k [[k4]]
    k = i >> j;

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpNot %int [[a5]]
// CHECK-NEXT: OpStore %b [[b5]]
    b = ~a;
// CHECK-NEXT: [[i5:%\d+]] = OpLoad %v3uint %i
// CHECK-NEXT: [[j5:%\d+]] = OpNot %v3uint [[i5]]
// CHECK-NEXT: OpStore %j [[j5]]
    j = ~i;
}
