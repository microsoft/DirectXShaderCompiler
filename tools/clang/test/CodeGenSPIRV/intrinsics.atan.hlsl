// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'atan' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[atan_a:%\d+]] = OpExtInst %float [[glsl]] Atan [[a]]
// CHECK-NEXT: OpStore %result [[atan_a]]
  float a;
  result = atan(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[atan_b:%\d+]] = OpExtInst %float [[glsl]] Atan [[b]]
// CHECK-NEXT: OpStore %result [[atan_b]]
  float1 b;
  result = atan(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[atan_c:%\d+]] = OpExtInst %v3float [[glsl]] Atan [[c]]
// CHECK-NEXT: OpStore %result3 [[atan_c]]
  float3 c;
  result3 = atan(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[atan_d:%\d+]] = OpExtInst %float [[glsl]] Atan [[d]]
// CHECK-NEXT: OpStore %result [[atan_d]]
  float1x1 d;
  result = atan(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[atan_e:%\d+]] = OpExtInst %v2float [[glsl]] Atan [[e]]
// CHECK-NEXT: OpStore %result2 [[atan_e]]
  float1x2 e;
  result2 = atan(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[atan_f:%\d+]] = OpExtInst %v4float [[glsl]] Atan [[f]]
// CHECK-NEXT: OpStore %result4 [[atan_f]]
  float4x1 f;
  result4 = atan(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[atan_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Atan [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[atan_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Atan [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[atan_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Atan [[g_row2]]
// CHECK-NEXT: [[atan_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[atan_g_row0]] [[atan_g_row1]] [[atan_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[atan_matrix]]
  float3x2 g;
  result3x2 = atan(g);
}
