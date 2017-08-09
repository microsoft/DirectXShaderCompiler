// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'tan' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[tan_a:%\d+]] = OpExtInst %float [[glsl]] Tan [[a]]
// CHECK-NEXT: OpStore %result [[tan_a]]
  float a;
  result = tan(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[tan_b:%\d+]] = OpExtInst %float [[glsl]] Tan [[b]]
// CHECK-NEXT: OpStore %result [[tan_b]]
  float1 b;
  result = tan(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[tan_c:%\d+]] = OpExtInst %v3float [[glsl]] Tan [[c]]
// CHECK-NEXT: OpStore %result3 [[tan_c]]
  float3 c;
  result3 = tan(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[tan_d:%\d+]] = OpExtInst %float [[glsl]] Tan [[d]]
// CHECK-NEXT: OpStore %result [[tan_d]]
  float1x1 d;
  result = tan(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[tan_e:%\d+]] = OpExtInst %v2float [[glsl]] Tan [[e]]
// CHECK-NEXT: OpStore %result2 [[tan_e]]
  float1x2 e;
  result2 = tan(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[tan_f:%\d+]] = OpExtInst %v4float [[glsl]] Tan [[f]]
// CHECK-NEXT: OpStore %result4 [[tan_f]]
  float4x1 f;
  result4 = tan(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[tan_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Tan [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[tan_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Tan [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[tan_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Tan [[g_row2]]
// CHECK-NEXT: [[tan_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[tan_g_row0]] [[tan_g_row1]] [[tan_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[tan_matrix]]
  float3x2 g;
  result3x2 = tan(g);
}
