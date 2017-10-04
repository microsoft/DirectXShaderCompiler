// Run: %dxc -T ps_6_0 -E main

// CHECK:  [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float3 a, b;

// CHECK:        [[a:%\d+]] = OpLoad %v3float %a
// CHECK-NEXT:   [[b:%\d+]] = OpLoad %v3float %b
// CHECK-NEXT:     {{%\d+}} = OpExtInst %float [[glsl]] Distance [[a]] [[b]]
  float d = distance(a, b);
}
