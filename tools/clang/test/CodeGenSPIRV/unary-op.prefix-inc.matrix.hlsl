// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2f1:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3f1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1
void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // 1x1
    float1x1 a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[a1:%\d+]] = OpFAdd %float [[a0]] %float_1
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %float %a
// CHECK-NEXT: OpStore %b [[a2]]
    b = ++a;
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[a4:%\d+]] = OpFAdd %float [[a3]] %float_1
// CHECK-NEXT: OpStore %a [[a4]]
// CHECK-NEXT: OpStore %a [[b0]]
    ++a = b;

    // Mx1
    float2x1 c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v2float %c
// CHECK-NEXT: [[c1:%\d+]] = OpFAdd %v2float [[c0]] [[v2f1]]
// CHECK-NEXT: OpStore %c [[c1]]
// CHECK-NEXT: [[c2:%\d+]] = OpLoad %v2float %c
// CHECK-NEXT: OpStore %d [[c2]]
    d = ++c;
// CHECK-NEXT: [[d0:%\d+]] = OpLoad %v2float %d
// CHECK-NEXT: [[c3:%\d+]] = OpLoad %v2float %c
// CHECK-NEXT: [[c4:%\d+]] = OpFAdd %v2float [[c3]] [[v2f1]]
// CHECK-NEXT: OpStore %c [[c4]]
// CHECK-NEXT: OpStore %c [[d0]]
    ++c = d;

    // 1xN
    float1x3 e, f;
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %v3float %e
// CHECK-NEXT: [[e1:%\d+]] = OpFAdd %v3float [[e0]] [[v3f1]]
// CHECK-NEXT: OpStore %e [[e1]]
// CHECK-NEXT: [[e2:%\d+]] = OpLoad %v3float %e
// CHECK-NEXT: OpStore %f [[e2]]
    f = ++e;
// CHECK-NEXT: [[f0:%\d+]] = OpLoad %v3float %f
// CHECK-NEXT: [[e3:%\d+]] = OpLoad %v3float %e
// CHECK-NEXT: [[e4:%\d+]] = OpFAdd %v3float [[e3]] [[v3f1]]
// CHECK-NEXT: OpStore %e [[e4]]
// CHECK-NEXT: OpStore %e [[f0]]
    ++e = f;

    // MxN
    float2x3 g, h;
// CHECK-NEXT: [[g0:%\d+]] = OpLoad %mat2v3float %g
// CHECK-NEXT: [[g0v0:%\d+]] = OpCompositeExtract %v3float [[g0]] 0
// CHECK-NEXT: [[inc0:%\d+]] = OpFAdd %v3float [[g0v0]] [[v3f1]]
// CHECK-NEXT: [[g0v1:%\d+]] = OpCompositeExtract %v3float [[g0]] 1
// CHECK-NEXT: [[inc1:%\d+]] = OpFAdd %v3float [[g0v1]] [[v3f1]]
// CHECK-NEXT: [[g1:%\d+]] = OpCompositeConstruct %mat2v3float [[inc0]] [[inc1]]
// CHECK-NEXT: OpStore %g [[g1]]
// CHECK-NEXT: [[g2:%\d+]] = OpLoad %mat2v3float %g
// CHECK-NEXT: OpStore %h [[g2]]
    h = ++g;
// CHECK-NEXT: [[h0:%\d+]] = OpLoad %mat2v3float %h
// CHECK-NEXT: [[g3:%\d+]] = OpLoad %mat2v3float %g
// CHECK-NEXT: [[g3v0:%\d+]] = OpCompositeExtract %v3float [[g3]] 0
// CHECK-NEXT: [[inc2:%\d+]] = OpFAdd %v3float [[g3v0]] [[v3f1]]
// CHECK-NEXT: [[g3v1:%\d+]] = OpCompositeExtract %v3float [[g3]] 1
// CHECK-NEXT: [[inc3:%\d+]] = OpFAdd %v3float [[g3v1]] [[v3f1]]
// CHECK-NEXT: [[g4:%\d+]] = OpCompositeConstruct %mat2v3float [[inc2]] [[inc3]]
// CHECK-NEXT: OpStore %g [[g4]]
// CHECK-NEXT: OpStore %g [[h0]]
    ++g = h;
}
