// Run: %dxc -T vs_6_0 -E main

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float    a, sina, cosa;
  float4   b, sinb, cosb;
  float2x3 c, sinc, cosc;

// CHECK:        [[a0:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[sina:%\d+]] = OpExtInst %float [[glsl]] Sin [[a0]]
// CHECK-NEXT:                 OpStore %sina [[sina]]
// CHECK-NEXT:   [[a1:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[cosa:%\d+]] = OpExtInst %float [[glsl]] Cos [[a1]]
// CHECK-NEXT:                 OpStore %cosa [[cosa]]
  sincos(a, sina, cosa);

// CHECK:        [[b0:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT: [[sinb:%\d+]] = OpExtInst %v4float [[glsl]] Sin [[b0]]
// CHECK-NEXT:                 OpStore %sinb [[sinb]]
// CHECK-NEXT:   [[b1:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT: [[cosb:%\d+]] = OpExtInst %v4float [[glsl]] Cos [[b1]]
// CHECK-NEXT:                 OpStore %cosb [[cosb]]
  sincos(b, sinb, cosb);

// CHECK:             [[c0:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:   [[c0_row0:%\d+]] = OpCompositeExtract %v3float [[c0]] 0
// CHECK-NEXT: [[sinc_row0:%\d+]] = OpExtInst %v3float [[glsl]] Sin [[c0_row0]]
// CHECK-NEXT:   [[c0_row1:%\d+]] = OpCompositeExtract %v3float [[c0]] 1
// CHECK-NEXT: [[sinc_row1:%\d+]] = OpExtInst %v3float [[glsl]] Sin [[c0_row1]]
// CHECK-NEXT:      [[sinc:%\d+]] = OpCompositeConstruct %mat2v3float [[sinc_row0]] [[sinc_row1]]
// CHECK-NEXT:                      OpStore %sinc [[sinc]]
// CHECK-NEXT:        [[c1:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:   [[c1_row0:%\d+]] = OpCompositeExtract %v3float [[c1]] 0
// CHECK-NEXT: [[cosc_row0:%\d+]] = OpExtInst %v3float [[glsl]] Cos [[c1_row0]]
// CHECK-NEXT:   [[c1_row1:%\d+]] = OpCompositeExtract %v3float [[c1]] 1
// CHECK-NEXT: [[cosc_row1:%\d+]] = OpExtInst %v3float [[glsl]] Cos [[c1_row1]]
// CHECK-NEXT:      [[cosc:%\d+]] = OpCompositeConstruct %mat2v3float [[cosc_row0]] [[cosc_row1]]
// CHECK-NEXT:                      OpStore %cosc [[cosc]]
  sincos(c, sinc, cosc);
}
