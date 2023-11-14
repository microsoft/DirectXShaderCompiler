// RUN: %dxc -T ps_6_0 -E main

// CHECK:  [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float4 i, n;
  float eta;

// CHECK:        [[i:%\d+]] = OpLoad %v4float %i
// CHECK-NEXT:   [[n:%\d+]] = OpLoad %v4float %n
// CHECK-NEXT: [[eta:%\d+]] = OpLoad %float %eta
// CHECK-NEXT:     {{%\d+}} = OpExtInst %v4float [[glsl]] Refract [[i]] [[n]] [[eta]]
  float4 r = refract(i, n, eta);
}
