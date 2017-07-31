// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    int1 a, b, c, d;
    int2 i, j;
    uint3 o, p;
    float4 x, y;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[add0:%\d+]] = OpIAdd %int [[b0]] [[a0]]
// CHECK-NEXT: OpStore %b [[add0]]
    b += a;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[add1:%\d+]] = OpIAdd %v2int [[j0]] [[i0]]
// CHECK-NEXT: OpStore %j [[add1]]
    j += i;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[add2:%\d+]] = OpIAdd %v3uint [[p0]] [[o0]]
// CHECK-NEXT: OpStore %p [[add2]]
    p += o;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y0:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[add3:%\d+]] = OpFAdd %v4float [[y0]] [[x0]]
// CHECK-NEXT: OpStore %y [[add3]]
    y += x;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[sub0:%\d+]] = OpISub %int [[b1]] [[a1]]
// CHECK-NEXT: OpStore %b [[sub0]]
    b -= a;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[sub1:%\d+]] = OpISub %v2int [[j1]] [[i1]]
// CHECK-NEXT: OpStore %j [[sub1]]
    j -= i;
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[sub2:%\d+]] = OpISub %v3uint [[p1]] [[o1]]
// CHECK-NEXT: OpStore %p [[sub2]]
    p -= o;
// CHECK-NEXT: [[x1:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y1:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[sub3:%\d+]] = OpFSub %v4float [[y1]] [[x1]]
// CHECK-NEXT: OpStore %y [[sub3]]
    y -= x;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mul0:%\d+]] = OpIMul %int [[b2]] [[a2]]
// CHECK-NEXT: OpStore %b [[mul0]]
    b *= a;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[mul1:%\d+]] = OpIMul %v2int [[j2]] [[i2]]
// CHECK-NEXT: OpStore %j [[mul1]]
    j *= i;
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[mul2:%\d+]] = OpIMul %v3uint [[p2]] [[o2]]
// CHECK-NEXT: OpStore %p [[mul2]]
    p *= o;
// CHECK-NEXT: [[x2:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y2:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[mul3:%\d+]] = OpFMul %v4float [[y2]] [[x2]]
// CHECK-NEXT: OpStore %y [[mul3]]
    y *= x;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[div0:%\d+]] = OpSDiv %int [[b4]] [[a4]]
// CHECK-NEXT: OpStore %b [[div0]]
    b /= a;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[div1:%\d+]] = OpSDiv %v2int [[j4]] [[i4]]
// CHECK-NEXT: OpStore %j [[div1]]
    j /= i;
// CHECK-NEXT: [[o4:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[div2:%\d+]] = OpUDiv %v3uint [[p4]] [[o4]]
// CHECK-NEXT: OpStore %p [[div2]]
    p /= o;
// CHECK-NEXT: [[x4:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y4:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[div3:%\d+]] = OpFDiv %v4float [[y4]] [[x4]]
// CHECK-NEXT: OpStore %y [[div3]]
    y /= x;

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[mod0:%\d+]] = OpSRem %int [[b5]] [[a5]]
// CHECK-NEXT: OpStore %b [[mod0]]
    b %= a;
// CHECK-NEXT: [[i5:%\d+]] = OpLoad %v2int %i
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %v2int %j
// CHECK-NEXT: [[mod1:%\d+]] = OpSRem %v2int [[j5]] [[i5]]
// CHECK-NEXT: OpStore %j [[mod1]]
    j %= i;
// CHECK-NEXT: [[o5:%\d+]] = OpLoad %v3uint %o
// CHECK-NEXT: [[p5:%\d+]] = OpLoad %v3uint %p
// CHECK-NEXT: [[mod2:%\d+]] = OpUMod %v3uint [[p5]] [[o5]]
// CHECK-NEXT: OpStore %p [[mod2]]
    p %= o;
// CHECK-NEXT: [[x5:%\d+]] = OpLoad %v4float %x
// CHECK-NEXT: [[y5:%\d+]] = OpLoad %v4float %y
// CHECK-NEXT: [[mod3:%\d+]] = OpFRem %v4float [[y5]] [[x5]]
// CHECK-NEXT: OpStore %y [[mod3]]
    y %= x;
}
