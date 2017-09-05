// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'ceil' function can only operate on float, vector of float, and matrix of float.

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float result;
  float2 result2;
  float3 result3;
  float4 result4;
  float3x2 result3x2;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[ceil_a:%\d+]] = OpExtInst %float [[glsl]] Ceil [[a]]
// CHECK-NEXT: OpStore %result [[ceil_a]]
  float a;
  result = ceil(a);

// CHECK-NEXT: [[b:%\d+]] = OpLoad %float %b
// CHECK-NEXT: [[ceil_b:%\d+]] = OpExtInst %float [[glsl]] Ceil [[b]]
// CHECK-NEXT: OpStore %result [[ceil_b]]
  float1 b;
  result = ceil(b);

// CHECK-NEXT: [[c:%\d+]] = OpLoad %v3float %c
// CHECK-NEXT: [[ceil_c:%\d+]] = OpExtInst %v3float [[glsl]] Ceil [[c]]
// CHECK-NEXT: OpStore %result3 [[ceil_c]]
  float3 c;
  result3 = ceil(c);

// CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
// CHECK-NEXT: [[ceil_d:%\d+]] = OpExtInst %float [[glsl]] Ceil [[d]]
// CHECK-NEXT: OpStore %result [[ceil_d]]
  float1x1 d;
  result = ceil(d);

// CHECK-NEXT: [[e:%\d+]] = OpLoad %v2float %e
// CHECK-NEXT: [[ceil_e:%\d+]] = OpExtInst %v2float [[glsl]] Ceil [[e]]
// CHECK-NEXT: OpStore %result2 [[ceil_e]]
  float1x2 e;
  result2 = ceil(e);

// CHECK-NEXT: [[f:%\d+]] = OpLoad %v4float %f
// CHECK-NEXT: [[ceil_f:%\d+]] = OpExtInst %v4float [[glsl]] Ceil [[f]]
// CHECK-NEXT: OpStore %result4 [[ceil_f]]
  float4x1 f;
  result4 = ceil(f);

// CHECK-NEXT: [[g:%\d+]] = OpLoad %mat3v2float %g
// CHECK-NEXT: [[g_row0:%\d+]] = OpCompositeExtract %v2float [[g]] 0
// CHECK-NEXT: [[ceil_g_row0:%\d+]] = OpExtInst %v2float [[glsl]] Ceil [[g_row0]]
// CHECK-NEXT: [[g_row1:%\d+]] = OpCompositeExtract %v2float [[g]] 1
// CHECK-NEXT: [[ceil_g_row1:%\d+]] = OpExtInst %v2float [[glsl]] Ceil [[g_row1]]
// CHECK-NEXT: [[g_row2:%\d+]] = OpCompositeExtract %v2float [[g]] 2
// CHECK-NEXT: [[ceil_g_row2:%\d+]] = OpExtInst %v2float [[glsl]] Ceil [[g_row2]]
// CHECK-NEXT: [[ceil_matrix:%\d+]] = OpCompositeConstruct %mat3v2float [[ceil_g_row0]] [[ceil_g_row1]] [[ceil_g_row2]]
// CHECK-NEXT: OpStore %result3x2 [[ceil_matrix]]
  float3x2 g;
  result3x2 = ceil(g);
}
