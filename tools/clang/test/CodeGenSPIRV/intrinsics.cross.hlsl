// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'cross' function can only operate on float3 vectors.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float3 a,b,c;

// CHECK:      [[a:%\d+]] = OpLoad %v3float %a
// CHECK-NEXT: [[b:%\d+]] = OpLoad %v3float %b
// CHECK-NEXT: [[c:%\d+]] = OpExtInst %v3float [[glsl]] Cross [[a]] [[b]]
  c = cross(a,b);
}
