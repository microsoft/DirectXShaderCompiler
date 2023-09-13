// RUN: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
// CHECK:      [[swizzle:%\d+]] = OpLoad %v3int %v3i
// CHECK-NEXT:  [[vector:%\d+]] = OpVectorShuffle %v3int [[swizzle]] {{%\d+}} 3 4 2
// CHECK-NEXT:                    OpStore %v3i [[vector]]
  float2 v2f;
  int3 v3i = 0;
  modf(v2f, v3i.xy);
}
