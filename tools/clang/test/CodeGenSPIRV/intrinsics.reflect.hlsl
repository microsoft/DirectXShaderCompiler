// Run: %dxc -T vs_6_0 -E main

// HLSL reflect() only operates on vectors of floats.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float4 a1,a2,result;

// CHECK: {{%\d+}} = OpExtInst %v4float [[glsl]] Reflect {{%\d+}} {{%\d+}}
  result = reflect(a1,a2);
}
