// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'acos' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[acos_a:%\d+]] = OpExtInst %float [[glsl]] Acos [[a]]
// CHECK-NEXT: OpStore %result [[acos_a]]
  float a;
  result = acos(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[acos_b:%\d+]] = OpExtInst %float [[glsl]] Acos [[b]]
// CHECK-NEXT: OpStore %result [[acos_b]]
  float1 b;
  result = acos(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[acos_c:%\d+]] = OpExtInst %v3float [[glsl]] Acos [[c]]
// CHECK-NEXT: OpStore %result3 [[acos_c]]
  float3 c;
  result3 = acos(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[acos_d:%\d+]] = OpExtInst %float [[glsl]] Acos [[d]]
// CHECK-NEXT: OpStore %result [[acos_d]]
  float1x1 d;
  result = acos(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[acos_e:%\d+]] = OpExtInst %v2float [[glsl]] Acos [[e]]
// CHECK-NEXT: OpStore %result2 [[acos_e]]
  float1x2 e;
  result2 = acos(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[acos_f:%\d+]] = OpExtInst %v4float [[glsl]] Acos [[f]]
// CHECK-NEXT: OpStore %result4 [[acos_f]]
  float4x1 f;
  result4 = acos(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[acos_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Acos [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[acos_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Acos [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[acos_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Acos [[g_row2]]
// CHECK-NEXT: [[acos_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[acos_g_row0]] [[acos_g_row1]] [[acos_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[acos_matrix]]
  float3x2 g;
  result3x2 = acos(g);
}
