// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'log' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[log_a:%\d+]] = OpExtInst %float [[glsl]] Log [[a]]
// CHECK-NEXT: OpStore %result [[log_a]]
  float a;
  result = log(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[log_b:%\d+]] = OpExtInst %float [[glsl]] Log [[b]]
// CHECK-NEXT: OpStore %result [[log_b]]
  float1 b;
  result = log(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[log_c:%\d+]] = OpExtInst %v3float [[glsl]] Log [[c]]
// CHECK-NEXT: OpStore %result3 [[log_c]]
  float3 c;
  result3 = log(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[log_d:%\d+]] = OpExtInst %float [[glsl]] Log [[d]]
// CHECK-NEXT: OpStore %result [[log_d]]
  float1x1 d;
  result = log(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[log_e:%\d+]] = OpExtInst %v2float [[glsl]] Log [[e]]
// CHECK-NEXT: OpStore %result2 [[log_e]]
  float1x2 e;
  result2 = log(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[log_f:%\d+]] = OpExtInst %v4float [[glsl]] Log [[f]]
// CHECK-NEXT: OpStore %result4 [[log_f]]
  float4x1 f;
  result4 = log(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[log_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Log [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[log_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Log [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[log_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Log [[g_row2]]
// CHECK-NEXT: [[log_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[log_g_row0]] [[log_g_row1]] [[log_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[log_matrix]]
  float3x2 g;
  result3x2 = log(g);
}
