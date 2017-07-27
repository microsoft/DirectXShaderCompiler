// Run: %dxc -T ps_6_0 -E main

void main() {
    int a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a1:%\d+]] = OpIAdd %int [[a0]] %int_1
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: OpStore %b [[a2]]
    b = ++a;
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a4:%\d+]] = OpIAdd %int [[a3]] %int_1
// CHECK-NEXT: OpStore %a [[a4]]
// CHECK-NEXT: OpStore %a [[b0]]
    ++a = b;

// Spot check a complicated usage case. No need to duplicate it for all types.

// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[b2:%\d+]] = OpIAdd %int [[b1]] %int_1
// CHECK-NEXT: OpStore %b [[b2]]
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[b4:%\d+]] = OpIAdd %int [[b3]] %int_1
// CHECK-NEXT: OpStore %b [[b4]]
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a6:%\d+]] = OpIAdd %int [[a5]] %int_1
// CHECK-NEXT: OpStore %a [[a6]]
// CHECK-NEXT: [[a7:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a8:%\d+]] = OpIAdd %int [[a7]] %int_1
// CHECK-NEXT: OpStore %a [[a8]]
// CHECK-NEXT: OpStore %a [[b5]]
    ++(++a) = ++(++b);

    uint i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[i1:%\d+]] = OpIAdd %uint [[i0]] %uint_1
// CHECK-NEXT: OpStore %i [[i1]]
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: OpStore %j [[i2]]
    j = ++i;
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[i4:%\d+]] = OpIAdd %uint [[i3]] %uint_1
// CHECK-NEXT: OpStore %i [[i4]]
// CHECK-NEXT: OpStore %i [[j0]]
    ++i = j;

    float o, p;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[01:%\d+]] = OpFAdd %float [[o0]] %float_1
// CHECK-NEXT: OpStore %o [[01]]
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %float %o
// CHECK-NEXT: OpStore %p [[o2]]
    p = ++o;
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[o3:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[o4:%\d+]] = OpFAdd %float [[o3]] %float_1
// CHECK-NEXT: OpStore %o [[o4]]
// CHECK-NEXT: OpStore %o [[p0]]
    ++o = p;
}
