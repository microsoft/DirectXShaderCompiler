// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'exp' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[exp_a:%\d+]] = OpExtInst %float [[glsl]] Exp [[a]]
// CHECK-NEXT: OpStore %result [[exp_a]]
  float a;
  result = exp(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[exp_b:%\d+]] = OpExtInst %float [[glsl]] Exp [[b]]
// CHECK-NEXT: OpStore %result [[exp_b]]
  float1 b;
  result = exp(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[exp_c:%\d+]] = OpExtInst %v3float [[glsl]] Exp [[c]]
// CHECK-NEXT: OpStore %result3 [[exp_c]]
  float3 c;
  result3 = exp(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[exp_d:%\d+]] = OpExtInst %float [[glsl]] Exp [[d]]
// CHECK-NEXT: OpStore %result [[exp_d]]
  float1x1 d;
  result = exp(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[exp_e:%\d+]] = OpExtInst %v2float [[glsl]] Exp [[e]]
// CHECK-NEXT: OpStore %result2 [[exp_e]]
  float1x2 e;
  result2 = exp(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[exp_f:%\d+]] = OpExtInst %v4float [[glsl]] Exp [[f]]
// CHECK-NEXT: OpStore %result4 [[exp_f]]
  float4x1 f;
  result4 = exp(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[exp_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Exp [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[exp_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Exp [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[exp_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Exp [[g_row2]]
// CHECK-NEXT: [[exp_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[exp_g_row0]] [[exp_g_row1]] [[exp_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[exp_matrix]]
  float3x2 g;
  result3x2 = exp(g);
}
