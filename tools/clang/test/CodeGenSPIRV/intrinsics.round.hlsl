// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'round' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[round_a:%\d+]] = OpExtInst %float [[glsl]] Round [[a]]
// CHECK-NEXT: OpStore %result [[round_a]]
  float a;
  result = round(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[round_b:%\d+]] = OpExtInst %float [[glsl]] Round [[b]]
// CHECK-NEXT: OpStore %result [[round_b]]
  float1 b;
  result = round(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[round_c:%\d+]] = OpExtInst %v3float [[glsl]] Round [[c]]
// CHECK-NEXT: OpStore %result3 [[round_c]]
  float3 c;
  result3 = round(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[round_d:%\d+]] = OpExtInst %float [[glsl]] Round [[d]]
// CHECK-NEXT: OpStore %result [[round_d]]
  float1x1 d;
  result = round(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[round_e:%\d+]] = OpExtInst %v2float [[glsl]] Round [[e]]
// CHECK-NEXT: OpStore %result2 [[round_e]]
  float1x2 e;
  result2 = round(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[round_f:%\d+]] = OpExtInst %v4float [[glsl]] Round [[f]]
// CHECK-NEXT: OpStore %result4 [[round_f]]
  float4x1 f;
  result4 = round(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[round_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Round [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[round_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Round [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[round_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Round [[g_row2]]
// CHECK-NEXT: [[round_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[round_g_row0]] [[round_g_row1]] [[round_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[round_matrix]]
  float3x2 g;
  result3x2 = round(g);
}
