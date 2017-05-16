// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int1 a, b, c;
    int2 i, j, k;
    uint3 o, p, q;
    float4 x, y, z;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[add0:%\d+]] = OpIAdd %int [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[add0]]
    c = a + b;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[add1:%\d+]] = OpIAdd %v2int [[i0]] [[j0]]
// CHECK-NEXT: OpStore %k [[add1]]
    k = i + j;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[add2:%\d+]] = OpIAdd %v3uint [[o0]] [[p0]]
// CHECK-NEXT: OpStore %q [[add2]]
    q = o + p;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[z0:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[add3:%\d+]] = OpFAdd %v4float [[x0]] [[z0]]
// CHECK-NEXT: OpStore %z [[add3]]
    z = x + y;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[sub0:%\d+]] = OpISub %int [[a1]] [[b1]]
// CHECK-NEXT: OpStore %c [[sub0]]
    c = a - b;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[sub1:%\d+]] = OpISub %v2int [[i1]] [[j1]]
// CHECK-NEXT: OpStore %k [[sub1]]
    k = i - j;
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[sub2:%\d+]] = OpISub %v3uint [[o1]] [[p1]]
// CHECK-NEXT: OpStore %q [[sub2]]
    q = o - p;
// CHECK-NEXT: [[x1:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y1:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[sub3:%\d+]] = OpFSub %v4float [[x1]] [[y1]]
// CHECK-NEXT: OpStore %z [[sub3]]
    z = x - y;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mul0:%\d+]] = OpIMul %int [[a2]] [[b2]]
// CHECK-NEXT: OpStore %c [[mul0]]
    c = a * b;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[mul1:%\d+]] = OpIMul %v2int [[i2]] [[j2]]
// CHECK-NEXT: OpStore %k [[mul1]]
    k = i * j;
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3uint [[o2]] [[p2]]
// CHECK-NEXT: OpStore %q [[mul2]]
    q = o * p;
// CHECK-NEXT: [[x2:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y2:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[mul3:%\d+]] = OpFMul %v4float [[x2]] [[y2]]
// CHECK-NEXT: OpStore %z [[mul3]]
    z = x * y;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[div0:%\d+]] = OpSDiv %int [[a4]] [[b4]]
// CHECK-NEXT: OpStore %c [[div0]]
    c = a / b;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[div1:%\d+]] = OpSDiv %v2int [[i4]] [[j4]]
// CHECK-NEXT: OpStore %k [[div1]]
    k = i / j;
// CHECK-NEXT: [[o4:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[div2:%\d+]] = OpUDiv %v3uint [[o4]] [[p4]]
// CHECK-NEXT: OpStore %q [[div2]]
    q = o / p;
// CHECK-NEXT: [[x4:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y4:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[div3:%\d+]] = OpFDiv %v4float [[x4]] [[y4]]
// CHECK-NEXT: OpStore %z [[div3]]
    z = x / y;

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mod0:%\d+]] = OpSRem %int [[a5]] [[b5]]
// CHECK-NEXT: OpStore %c [[mod0]]
    c = a % b;
// CHECK-NEXT: [[i5:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[mod1:%\d+]] = OpSRem %v2int [[i5]] [[j5]]
// CHECK-NEXT: OpStore %k [[mod1]]
    k = i % j;
// CHECK-NEXT: [[o5:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p5:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[mod2:%\d+]] = OpUMod %v3uint [[o5]] [[p5]]
// CHECK-NEXT: OpStore %q [[mod2]]
    q = o % p;
// CHECK-NEXT: [[x5:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y5:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[mod3:%\d+]] = OpFRem %v4float [[x5]] [[y5]]
// CHECK-NEXT: OpStore %z [[mod3]]
    z = x % y;
}
