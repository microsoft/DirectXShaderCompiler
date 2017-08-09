// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'normalize' function can only operate on floats and vector of floats.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[normalize_a:%\d+]] = OpExtInst %float [[glsl]] Normalize [[a]]
// CHECK-NEXT: OpStore %result [[normalize_a]]
  float a;
  result = normalize(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[normalize_b:%\d+]] = OpExtInst %float [[glsl]] Normalize [[b]]
// CHECK-NEXT: OpStore %result [[normalize_b]]
  float1 b;
  result = normalize(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[normalize_c:%\d+]] = OpExtInst %v3float [[glsl]] Normalize [[c]]
// CHECK-NEXT: OpStore %result3 [[normalize_c]]
  float3 c;
  result3 = normalize(c);
}
