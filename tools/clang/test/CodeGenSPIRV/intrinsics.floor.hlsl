// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'floor' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[floor_a:%\d+]] = OpExtInst %float [[glsl]] Floor [[a]]
// CHECK-NEXT: OpStore %result [[floor_a]]
  float a;
  result = floor(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[floor_b:%\d+]] = OpExtInst %float [[glsl]] Floor [[b]]
// CHECK-NEXT: OpStore %result [[floor_b]]
  float1 b;
  result = floor(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[floor_c:%\d+]] = OpExtInst %v3float [[glsl]] Floor [[c]]
// CHECK-NEXT: OpStore %result3 [[floor_c]]
  float3 c;
  result3 = floor(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[floor_d:%\d+]] = OpExtInst %float [[glsl]] Floor [[d]]
// CHECK-NEXT: OpStore %result [[floor_d]]
  float1x1 d;
  result = floor(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[floor_e:%\d+]] = OpExtInst %v2float [[glsl]] Floor [[e]]
// CHECK-NEXT: OpStore %result2 [[floor_e]]
  float1x2 e;
  result2 = floor(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[floor_f:%\d+]] = OpExtInst %v4float [[glsl]] Floor [[f]]
// CHECK-NEXT: OpStore %result4 [[floor_f]]
  float4x1 f;
  result4 = floor(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[floor_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Floor [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[floor_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Floor [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[floor_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Floor [[g_row2]]
// CHECK-NEXT: [[floor_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[floor_g_row0]] [[floor_g_row1]] [[floor_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[floor_matrix]]
  float3x2 g;
  result3x2 = floor(g);
}
