// Run: %dxc -T vs_6_0 -E main

void main() {
  float    a1, a2, fmod_a;
  float4   b1, b2, fmod_b;
  float2x3 c1, c2, fmod_c;

// CHECK:      [[a1:%\d+]] = OpLoad %float %a1
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %float %a2
// CHECK-NEXT:    {{%\d+}} = OpFRem %float [[a1]] [[a2]]
  fmod_a = fmod(a1, a2);

// CHECK:      [[b1:%\d+]] = OpLoad %v4float %b1
// CHECK-NEXT: [[b2:%\d+]] = OpLoad %v4float %b2
// CHECK-NEXT:    {{%\d+}} = OpFRem %v4float [[b1]] [[b2]]
  fmod_b = fmod(b1, b2);

// CHECK:               [[c1:%\d+]] = OpLoad %mat2v3float %c1
// CHECK-NEXT:          [[c2:%\d+]] = OpLoad %mat2v3float %c2
// CHECK-NEXT:     [[c1_row0:%\d+]] = OpCompositeExtract %v3float [[c1]] 0
// CHECK-NEXT:     [[c2_row0:%\d+]] = OpCompositeExtract %v3float [[c2]] 0
// CHECK-NEXT: [[fmod_c_row0:%\d+]] = OpFRem %v3float [[c1_row0]] [[c2_row0]]
// CHECK-NEXT:     [[c1_row1:%\d+]] = OpCompositeExtract %v3float [[c1]] 1
// CHECK-NEXT:     [[c2_row1:%\d+]] = OpCompositeExtract %v3float [[c2]] 1
// CHECK-NEXT: [[fmod_c_row1:%\d+]] = OpFRem %v3float [[c1_row1]] [[c2_row1]]
// CHECK-NEXT:             {{%\d+}} = OpCompositeConstruct %mat2v3float [[fmod_c_row0]] [[fmod_c_row1]]
  fmod_c = fmod(c1, c2);
}
