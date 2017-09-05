// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int a, b, c, d;
    uint i, j;
    float o, p;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[add0:%\d+]] = OpIAdd %int [[b0]] [[a0]]
// CHECK-NEXT: OpStore %b [[add0]]
    b += a;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[add1:%\d+]] = OpIAdd %uint [[j0]] [[i0]]
// CHECK-NEXT: OpStore %j [[add1]]
    j += i;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[add2:%\d+]] = OpFAdd %float [[p0]] [[o0]]
// CHECK-NEXT: OpStore %p [[add2]]
    p += o;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[sub0:%\d+]] = OpISub %int [[b1]] [[a1]]
// CHECK-NEXT: OpStore %b [[sub0]]
    b -= a;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[sub1:%\d+]] = OpISub %uint [[j1]] [[i1]]
// CHECK-NEXT: OpStore %j [[sub1]]
    j -= i;
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[sub2:%\d+]] = OpFSub %float [[p1]] [[o1]]
// CHECK-NEXT: OpStore %p [[sub2]]
    p -= o;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mul0:%\d+]] = OpIMul %int [[b2]] [[a2]]
// CHECK-NEXT: OpStore %b [[mul0]]
    b *= a;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[mul1:%\d+]] = OpIMul %uint [[j2]] [[i2]]
// CHECK-NEXT: OpStore %j [[mul1]]
    j *= i;
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[mul2:%\d+]] = OpFMul %float [[p2]] [[o2]]
// CHECK-NEXT: OpStore %p [[mul2]]
    p *= o;

// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[div0:%\d+]] = OpSDiv %int [[b3]] [[a3]]
// CHECK-NEXT: OpStore %b [[div0]]
    b /= a;
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[div1:%\d+]] = OpUDiv %uint [[j3]] [[i3]]
// CHECK-NEXT: OpStore %j [[div1]]
    j /= i;
// CHECK-NEXT: [[o3:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p3:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[div2:%\d+]] = OpFDiv %float [[p3]] [[o3]]
// CHECK-NEXT: OpStore %p [[div2]]
    p /= o;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mod0:%\d+]] = OpSRem %int [[b4]] [[a4]]
// CHECK-NEXT: OpStore %b [[mod0]]
    b %= a;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[mod1:%\d+]] = OpUMod %uint [[j4]] [[i4]]
// CHECK-NEXT: OpStore %j [[mod1]]
    j %= i;
// CHECK-NEXT: [[o4:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[mod2:%\d+]] = OpFRem %float [[p4]] [[o4]]
// CHECK-NEXT: OpStore %p [[mod2]]
    p %= o;

    // Spot check that we can use the result on both the left-hand side
    // and the right-hand side
// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[sub3:%\d+]] = OpISub %int [[b5]] [[a5]]
// CHECK-NEXT: OpStore %b [[sub3]]
// CHECK-NEXT: [[b6:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %int %c
// CHECK-NEXT: [[d0:%\d+]] = OpLoad %int %d
// CHECK-NEXT: [[add3:%\d+]] = OpIAdd %int [[d0]] [[c0]]
// CHECK-NEXT: OpStore %d [[add3]]
// CHECK-NEXT: OpStore %d [[b6]]
    (d += c) = (b -= a);
}
