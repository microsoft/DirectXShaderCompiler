// RUN: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float    a, frac_a;
  float4   b, frac_b;
  float2x3 c, frac_c;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpExtInst %float [[glsl]] Fract [[a]]
  frac_a = frac(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpExtInst %v4float [[glsl]] Fract [[b]]
  frac_b = frac(b);

// CHECK:               [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:     [[c_row0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT:[[frac_c_row0:%\d+]] = OpExtInst %v3float [[glsl]] Fract [[c_row0]]
// CHECK-NEXT:     [[c_row1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT:[[frac_c_row1:%\d+]] = OpExtInst %v3float [[glsl]] Fract [[c_row1]]
// CHECK-NEXT:            {{%\d+}} = OpCompositeConstruct %mat2v3float [[frac_c_row0]] [[frac_c_row1]]
  frac_c = frac(c);
}
