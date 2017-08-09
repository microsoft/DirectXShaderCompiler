// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'degrees' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[degrees_a:%\d+]] = OpExtInst %float [[glsl]] Degrees [[a]]
// CHECK-NEXT: OpStore %result [[degrees_a]]
  float a;
  result = degrees(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[degrees_b:%\d+]] = OpExtInst %float [[glsl]] Degrees [[b]]
// CHECK-NEXT: OpStore %result [[degrees_b]]
  float1 b;
  result = degrees(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[degrees_c:%\d+]] = OpExtInst %v3float [[glsl]] Degrees [[c]]
// CHECK-NEXT: OpStore %result3 [[degrees_c]]
  float3 c;
  result3 = degrees(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[degrees_d:%\d+]] = OpExtInst %float [[glsl]] Degrees [[d]]
// CHECK-NEXT: OpStore %result [[degrees_d]]
  float1x1 d;
  result = degrees(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[degrees_e:%\d+]] = OpExtInst %v2float [[glsl]] Degrees [[e]]
// CHECK-NEXT: OpStore %result2 [[degrees_e]]
  float1x2 e;
  result2 = degrees(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[degrees_f:%\d+]] = OpExtInst %v4float [[glsl]] Degrees [[f]]
// CHECK-NEXT: OpStore %result4 [[degrees_f]]
  float4x1 f;
  result4 = degrees(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[degrees_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Degrees [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[degrees_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Degrees [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[degrees_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Degrees [[g_row2]]
// CHECK-NEXT: [[degrees_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[degrees_g_row0]] [[degrees_g_row1]] [[degrees_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[degrees_matrix]]
  float3x2 g;
  result3x2 = degrees(g);
}
