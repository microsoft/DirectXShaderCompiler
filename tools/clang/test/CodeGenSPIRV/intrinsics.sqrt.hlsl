// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'sqrt' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[sqrt_a:%\d+]] = OpExtInst %float [[glsl]] Sqrt [[a]]
// CHECK-NEXT: OpStore %result [[sqrt_a]]
  float a;
  result = sqrt(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[sqrt_b:%\d+]] = OpExtInst %float [[glsl]] Sqrt [[b]]
// CHECK-NEXT: OpStore %result [[sqrt_b]]
  float1 b;
  result = sqrt(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[sqrt_c:%\d+]] = OpExtInst %v3float [[glsl]] Sqrt [[c]]
// CHECK-NEXT: OpStore %result3 [[sqrt_c]]
  float3 c;
  result3 = sqrt(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[sqrt_d:%\d+]] = OpExtInst %float [[glsl]] Sqrt [[d]]
// CHECK-NEXT: OpStore %result [[sqrt_d]]
  float1x1 d;
  result = sqrt(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[sqrt_e:%\d+]] = OpExtInst %v2float [[glsl]] Sqrt [[e]]
// CHECK-NEXT: OpStore %result2 [[sqrt_e]]
  float1x2 e;
  result2 = sqrt(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[sqrt_f:%\d+]] = OpExtInst %v4float [[glsl]] Sqrt [[f]]
// CHECK-NEXT: OpStore %result4 [[sqrt_f]]
  float4x1 f;
  result4 = sqrt(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[sqrt_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Sqrt [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[sqrt_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Sqrt [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[sqrt_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Sqrt [[g_row2]]
// CHECK-NEXT: [[sqrt_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[sqrt_g_row0]] [[sqrt_g_row1]] [[sqrt_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[sqrt_matrix]]
  float3x2 g;
  result3x2 = sqrt(g);
}
