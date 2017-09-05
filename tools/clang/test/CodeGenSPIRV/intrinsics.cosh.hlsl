// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'cosh' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[cosh_a:%\d+]] = OpExtInst %float [[glsl]] Cosh [[a]]
// CHECK-NEXT: OpStore %result [[cosh_a]]
  float a;
  result = cosh(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[cosh_b:%\d+]] = OpExtInst %float [[glsl]] Cosh [[b]]
// CHECK-NEXT: OpStore %result [[cosh_b]]
  float1 b;
  result = cosh(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[cosh_c:%\d+]] = OpExtInst %v3float [[glsl]] Cosh [[c]]
// CHECK-NEXT: OpStore %result3 [[cosh_c]]
  float3 c;
  result3 = cosh(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[cosh_d:%\d+]] = OpExtInst %float [[glsl]] Cosh [[d]]
// CHECK-NEXT: OpStore %result [[cosh_d]]
  float1x1 d;
  result = cosh(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[cosh_e:%\d+]] = OpExtInst %v2float [[glsl]] Cosh [[e]]
// CHECK-NEXT: OpStore %result2 [[cosh_e]]
  float1x2 e;
  result2 = cosh(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[cosh_f:%\d+]] = OpExtInst %v4float [[glsl]] Cosh [[f]]
// CHECK-NEXT: OpStore %result4 [[cosh_f]]
  float4x1 f;
  result4 = cosh(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[cosh_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Cosh [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[cosh_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Cosh [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[cosh_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Cosh [[g_row2]]
// CHECK-NEXT: [[cosh_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[cosh_g_row0]] [[cosh_g_row1]] [[cosh_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[cosh_matrix]]
  float3x2 g;
  result3x2 = cosh(g);
}
