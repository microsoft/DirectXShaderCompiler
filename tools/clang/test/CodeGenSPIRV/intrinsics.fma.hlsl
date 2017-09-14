// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'fma' function can only operate on double, vector of double, and matrix of double.

// CHECK:                 OpCapability Float64
// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"
// CHECK:       %double = OpTypeFloat 64
// CHECK:     %v3double = OpTypeVector %double 3
// CHECK: %mat2v3double = OpTypeMatrix %v3double 2

void main() {
  double    a1, a2, a3, fma_a;
  double4   b1, b2, b3, fma_b;
  double2x3 c1, c2, c3, fma_c;

// CHECK:      [[a1:%\d+]] = OpLoad %double %a1
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %double %a2
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %double %a3
// CHECK-NEXT:    {{%\d+}} = OpExtInst %double [[glsl]] Fma [[a1]] [[a2]] [[a3]]
  fma_a = fma(a1, a2, a3);

// CHECK:      [[b1:%\d+]] = OpLoad %v4double %b1
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %v4double %b2
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %v4double %b3
// CHECK-NEXT:    {{%\d+}} = OpExtInst %v4double [[glsl]] Fma [[b1]] [[b2]] [[b3]]
  fma_b = fma(b1, b2, b3);

// CHECK:            [[c1:%\d+]] = OpLoad %mat2v3double %c1
// CHECK-NEXT:       [[c2:%\d+]] = OpLoad %mat2v3double %c2
// CHECK-NEXT:       [[c3:%\d+]] = OpLoad %mat2v3double %c3
// CHECK-NEXT:  [[c1_row0:%\d+]] = OpCompositeExtract %v3double [[c1]] 0
// CHECK-NEXT:  [[c2_row0:%\d+]] = OpCompositeExtract %v3double [[c2]] 0
// CHECK-NEXT:  [[c3_row0:%\d+]] = OpCompositeExtract %v3double [[c3]] 0
// CHECK-NEXT: [[fma_row0:%\d+]] = OpExtInst %v3double [[glsl]] Fma [[c1_row0]] [[c2_row0]] [[c3_row0]]
// CHECK-NEXT:  [[c1_row1:%\d+]] = OpCompositeExtract %v3double [[c1]] 1
// CHECK-NEXT:  [[c2_row1:%\d+]] = OpCompositeExtract %v3double [[c2]] 1
// CHECK-NEXT:  [[c3_row1:%\d+]] = OpCompositeExtract %v3double [[c3]] 1
// CHECK-NEXT: [[fma_row1:%\d+]] = OpExtInst %v3double [[glsl]] Fma [[c1_row1]] [[c2_row1]] [[c3_row1]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %mat2v3double [[fma_row0]] [[fma_row1]]
  fma_c = fma(c1, c2, c3);
}
