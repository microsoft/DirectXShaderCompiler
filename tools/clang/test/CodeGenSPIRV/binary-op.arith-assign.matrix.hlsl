// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    float1x1 a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[b1:%\d+]] = OpFAdd %float [[b0]] [[a0]]
// CHECK-NEXT: OpStore %b [[b1]]
    b += a;

    float2x1 c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %v2float %c
// CHECK-NEXT: [[d0:%\d+]] = OpLoad %v2float %d
// CHECK-NEXT: [[d1:%\d+]] = OpFSub %v2float [[d0]] [[c0]]
// CHECK-NEXT: OpStore %d [[d1]]
    d -= c;

    float1x3 e, f;
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %v3float %e
// CHECK-NEXT: [[f0:%\d+]] = OpLoad %v3float %f
// CHECK-NEXT: [[f1:%\d+]] = OpFMul %v3float [[f0]] [[e0]]
// CHECK-NEXT: OpStore %f [[f1]]
    f *= e;

    float2x3 g, h;
// CHECK-NEXT: [[g0:%\d+]] = OpLoad %mat2v3float %g
// CHECK-NEXT: [[h0:%\d+]] = OpLoad %mat2v3float %h
// CHECK-NEXT: [[h0v0:%\d+]] = OpCompositeExtract %v3float [[h0]] 0
// CHECK-NEXT: [[g0v0:%\d+]] = OpCompositeExtract %v3float [[g0]] 0
// CHECK-NEXT: [[h1v0:%\d+]] = OpFDiv %v3float [[h0v0]] [[g0v0]]
// CHECK-NEXT: [[h0v1:%\d+]] = OpCompositeExtract %v3float [[h0]] 1
// CHECK-NEXT: [[g0v1:%\d+]] = OpCompositeExtract %v3float [[g0]] 1
// CHECK-NEXT: [[h1v1:%\d+]] = OpFDiv %v3float [[h0v1]] [[g0v1]]
// CHECK-NEXT: [[h1:%\d+]] = OpCompositeConstruct %mat2v3float [[h1v0]] [[h1v1]]
// CHECK-NEXT: OpStore %h [[h1]]
    h /= g;

    float3x2 i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %mat3v2float %i
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %mat3v2float %j
// CHECK-NEXT: [[j0v0:%\d+]] = OpCompositeExtract %v2float [[j0]] 0
// CHECK-NEXT: [[i0v0:%\d+]] = OpCompositeExtract %v2float [[i0]] 0
// CHECK-NEXT: [[j1v0:%\d+]] = OpFRem %v2float [[j0v0]] [[i0v0]]
// CHECK-NEXT: [[j0v1:%\d+]] = OpCompositeExtract %v2float [[j0]] 1
// CHECK-NEXT: [[i0v1:%\d+]] = OpCompositeExtract %v2float [[i0]] 1
// CHECK-NEXT: [[j1v1:%\d+]] = OpFRem %v2float [[j0v1]] [[i0v1]]
// CHECK-NEXT: [[j0v2:%\d+]] = OpCompositeExtract %v2float [[j0]] 2
// CHECK-NEXT: [[i0v2:%\d+]] = OpCompositeExtract %v2float [[i0]] 2
// CHECK-NEXT: [[j1v2:%\d+]] = OpFRem %v2float [[j0v2]] [[i0v2]]
// CHECK-NEXT: [[j1:%\d+]] = OpCompositeConstruct %mat3v2float [[j1v0]] [[j1v1]] [[j1v2]]
// CHECK-NEXT: OpStore %j [[j1]]
    j %= i;
}
