// RUN: %dxc -T ps_6_0 -E main

// faceforward only takes vector of floats, and returns a vector of the same size.

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float3 n, i, ng, result;
  
// CHECK:       [[n:%\d+]] = OpLoad %v3float %n
// CHECK-NEXT:  [[i:%\d+]] = OpLoad %v3float %i
// CHECK-NEXT: [[ng:%\d+]] = OpLoad %v3float %ng
// CHECK-NEXT: {{%\d+}} = OpExtInst %v3float [[glsl]] FaceForward [[n]] [[i]] [[ng]]
  result = faceforward(n, i, ng);
}
