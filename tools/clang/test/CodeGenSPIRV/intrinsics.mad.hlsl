// Run: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float    a1, a2, a3, fma_a;
  float4   b1, b2, b3, fma_b;
  float2x3 c1, c2, c3, fma_c;

// CHECK:      [[a1:%\d+]] = OpLoad %float %a1
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %float %a2
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %float %a3
// CHECK-NEXT:    {{%\d+}} = OpExtInst %float [[glsl]] Fma [[a1]] [[a2]] [[a3]]
  fma_a = mad(a1, a2, a3);

// CHECK:      [[b1:%\d+]] = OpLoad %v4float %b1
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %v4float %b2
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %v4float %b3
// CHECK-NEXT:    {{%\d+}} = OpExtInst %v4float [[glsl]] Fma [[b1]] [[b2]] [[b3]]
  fma_b = mad(b1, b2, b3);

// CHECK:            [[c1:%\d+]] = OpLoad %mat2v3float %c1
// CHECK-NEXT:       [[c2:%\d+]] = OpLoad %mat2v3float %c2
// CHECK-NEXT:       [[c3:%\d+]] = OpLoad %mat2v3float %c3
// CHECK-NEXT:  [[c1_row0:%\d+]] = OpCompositeExtract %v3float [[c1]] 0
// CHECK-NEXT:  [[c2_row0:%\d+]] = OpCompositeExtract %v3float [[c2]] 0
// CHECK-NEXT:  [[c3_row0:%\d+]] = OpCompositeExtract %v3float [[c3]] 0
// CHECK-NEXT: [[fma_row0:%\d+]] = OpExtInst %v3float [[glsl]] Fma [[c1_row0]] [[c2_row0]] [[c3_row0]]
// CHECK-NEXT:  [[c1_row1:%\d+]] = OpCompositeExtract %v3float [[c1]] 1
// CHECK-NEXT:  [[c2_row1:%\d+]] = OpCompositeExtract %v3float [[c2]] 1
// CHECK-NEXT:  [[c3_row1:%\d+]] = OpCompositeExtract %v3float [[c3]] 1
// CHECK-NEXT: [[fma_row1:%\d+]] = OpExtInst %v3float [[glsl]] Fma [[c1_row1]] [[c2_row1]] [[c3_row1]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %mat2v3float [[fma_row0]] [[fma_row1]]
  fma_c = mad(c1, c2, c3);
}
