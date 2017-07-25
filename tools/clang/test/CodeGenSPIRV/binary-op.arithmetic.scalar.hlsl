// Run: %dxc -T ps_6_0 -E main

void main() {
    int a, b, c;
    uint i, j, k;
    float o, p, q;

// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c0:%\d+]] = OpIAdd %int [[a0]] [[b0]]
// CHECK-NEXT: OpStore %c [[c0]]
    c = a + b;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[k0:%\d+]] = OpIAdd %uint [[i0]] [[j0]]
// CHECK-NEXT: OpStore %k [[k0]]
    k = i + j;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[q0:%\d+]] = OpFAdd %float [[o0]] [[p0]]
// CHECK-NEXT: OpStore %q [[q0]]
    q = o + p;

// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c1:%\d+]] = OpISub %int [[a1]] [[b1]]
// CHECK-NEXT: OpStore %c [[c1]]
    c = a - b;
// CHECK-NEXT: [[i1:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j1:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[k1:%\d+]] = OpISub %uint [[i1]] [[j1]]
// CHECK-NEXT: OpStore %k [[k1]]
    k = i - j;
// CHECK-NEXT: [[o1:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p1:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[q1:%\d+]] = OpFSub %float [[o1]] [[p1]]
// CHECK-NEXT: OpStore %q [[q1]]
    q = o - p;

// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c2:%\d+]] = OpIMul %int [[a2]] [[b2]]
// CHECK-NEXT: OpStore %c [[c2]]
    c = a * b;
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j2:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[k2:%\d+]] = OpIMul %uint [[i2]] [[j2]]
// CHECK-NEXT: OpStore %k [[k2]]
    k = i * j;
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p2:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[q2:%\d+]] = OpFMul %float [[o2]] [[p2]]
// CHECK-NEXT: OpStore %q [[q2]]
    q = o * p;

// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c3:%\d+]] = OpSDiv %int [[a3]] [[b3]]
// CHECK-NEXT: OpStore %c [[c3]]
    c = a / b;
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j3:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[k3:%\d+]] = OpUDiv %uint [[i3]] [[j3]]
// CHECK-NEXT: OpStore %k [[k3]]
    k = i / j;
// CHECK-NEXT: [[o3:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p3:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[q3:%\d+]] = OpFDiv %float [[o3]] [[p3]]
// CHECK-NEXT: OpStore %q [[q3]]
    q = o / p;

// CHECK-NEXT: [[a4:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b4:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[c4:%\d+]] = OpSRem %int [[a4]] [[b4]]
// CHECK-NEXT: OpStore %c [[c4]]
    c = a % b;
// CHECK-NEXT: [[i4:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j4:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[k4:%\d+]] = OpUMod %uint [[i4]] [[j4]]
// CHECK-NEXT: OpStore %k [[k4]]
    k = i % j;
// CHECK-NEXT: [[o4:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p4:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[q4:%\d+]] = OpFRem %float [[o4]] [[p4]]
// CHECK-NEXT: OpStore %q [[q4]]
    q = o % p;

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a6:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[in0:%\d+]] = OpIMul %int [[b5]] [[a6]]
// CHECK-NEXT: [[c5:%\d+]] = OpLoad %int %c
// CHECK-NEXT: [[in1:%\d+]] = OpSDiv %int [[in0]] [[c5]]
// CHECK-NEXT: [[b6:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[in2:%\d+]] = OpSRem %int [[in1]] [[b6]]
// CHECK-NEXT: [[in3:%\d+]] = OpIAdd %int [[a5]] [[in2]]
// CHECK-NEXT: OpStore %c [[in3]]
    c = a + b * a / c % b;
// CHECK-NEXT: [[i5:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[j5:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[i6:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[in4:%\d+]] = OpIMul %uint [[j5]] [[i6]]
// CHECK-NEXT: [[k5:%\d+]] = OpLoad %uint %k
// CHECK-NEXT: [[in5:%\d+]] = OpUDiv %uint [[in4]] [[k5]]
// CHECK-NEXT: [[j6:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[in6:%\d+]] = OpUMod %uint [[in5]] [[j6]]
// CHECK-NEXT: [[in7:%\d+]] = OpIAdd %uint [[i5]] [[in6]]
// CHECK-NEXT: OpStore %k [[in7]]
    k = i + j * i / k % j;
// CHECK-NEXT: [[o5:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[p5:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[o6:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[in8:%\d+]] = OpFMul %float [[p5]] [[o6]]
// CHECK-NEXT: [[q5:%\d+]] = OpLoad %float %q
// CHECK-NEXT: [[in9:%\d+]] = OpFDiv %float [[in8]] [[q5]]
// CHECK-NEXT: [[p6:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[in10:%\d+]] = OpFRem %float [[in9]] [[p6]]
// CHECK-NEXT: [[in11:%\d+]] = OpFAdd %float [[o5]] [[in10]]
// CHECK-NEXT: OpStore %q [[in11]]
    q = o + p * o / q % p;
}
