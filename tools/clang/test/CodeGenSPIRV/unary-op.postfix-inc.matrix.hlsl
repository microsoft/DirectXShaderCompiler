// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2f1:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3f1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1
// CHECK: [[v3i1:%\d+]] = OpConstantComposite %v3int %int_1 %int_1 %int_1

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // 1x1
    float1x1 a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[a1:%\d+]] = OpFAdd %float [[a0]] %float_1
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: OpStore %b [[a0]]
    b = a++;

    // Mx1
    float2x1 c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v2float %c
// CHECK-NEXT: [[c1:%\d+]] = OpFAdd %v2float [[c0]] [[v2f1]]
// CHECK-NEXT: OpStore %c [[c1]]
// CHECK-NEXT: OpStore %d [[c0]]
    d = c++;

    // 1xN
    float1x3 e, f;
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %v3float %e
// CHECK-NEXT: [[e1:%\d+]] = OpFAdd %v3float [[e0]] [[v3f1]]
// CHECK-NEXT: OpStore %e [[e1]]
// CHECK-NEXT: OpStore %f [[e0]]
    f = e++;

    // MxN
    float2x3 g, h;
// CHECK-NEXT: [[g0:%\d+]] = OpLoad %mat2v3float %g
// CHECK-NEXT: [[g0v0:%\d+]] = OpCompositeExtract %v3float [[g0]] 0
// CHECK-NEXT: [[inc0:%\d+]] = OpFAdd %v3float [[g0v0]] [[v3f1]]
// CHECK-NEXT: [[g0v1:%\d+]] = OpCompositeExtract %v3float [[g0]] 1
// CHECK-NEXT: [[inc1:%\d+]] = OpFAdd %v3float [[g0v1]] [[v3f1]]
// CHECK-NEXT: [[g1:%\d+]] = OpCompositeConstruct %mat2v3float [[inc0]] [[inc1]]
// CHECK-NEXT: OpStore %g [[g1]]
// CHECK-NEXT: OpStore %h [[g0]]
    h = g++;

// CHECK-NEXT: [[m0:%\d+]] = OpLoad %_arr_v3int_uint_2 %m
// CHECK-NEXT: [[m0v0:%\d+]] = OpCompositeExtract %v3int [[m0]] 0
// CHECK-NEXT: [[inc0:%\d+]] = OpIAdd %v3int [[m0v0]] [[v3i1]]
// CHECK-NEXT: [[m0v1:%\d+]] = OpCompositeExtract %v3int [[m0]] 1
// CHECK-NEXT: [[inc1:%\d+]] = OpIAdd %v3int [[m0v1]] [[v3i1]]
// CHECK-NEXT: [[m1:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[inc0]] [[inc1]]
// CHECK-NEXT: OpStore %m [[m1]]
// CHECK-NEXT: OpStore %n [[m0]]
    int2x3 m, n;
    n = m++;
}
