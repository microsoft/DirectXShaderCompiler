// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'log10' function can only operate on float, vector of float, and matrix of floats.

// CHECK:  [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"
// CHECK: %float_0_30103 = OpConstant %float 0.30103

void main() {
  float    a, log10_a;
  float4   b, log10_b;
  float2x3 c, log10_c;
  
// CHECK:           [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[log2_a:%\d+]] = OpExtInst %float [[glsl]] Log2 [[a]]
// CHECK-NEXT:[[log10_a:%\d+]] = OpFMul %float [[log2_a]] %float_0_30103
// CHECK-NEXT:                   OpStore %log10_a [[log10_a]]
  log10_a = log10(a);

// CHECK:           [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT: [[log2_b:%\d+]] = OpExtInst %v4float [[glsl]] Log2 [[b]]
// CHECK-NEXT:[[log10_b:%\d+]] = OpVectorTimesScalar %v4float [[log2_b]] %float_0_30103
// CHECK-NEXT:                   OpStore %log10_b [[log10_b]]
  log10_b = log10(b);

// CHECK:                [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:      [[c_row0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[log2_c_row0:%\d+]] = OpExtInst %v3float [[glsl]] Log2 [[c_row0]]
// CHECK-NEXT:      [[c_row1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[log2_c_row1:%\d+]] = OpExtInst %v3float [[glsl]] Log2 [[c_row1]]
// CHECK-NEXT:      [[log2_c:%\d+]] = OpCompositeConstruct %mat2v3float [[log2_c_row0]] [[log2_c_row1]]
// CHECK-NEXT:     [[log10_c:%\d+]] = OpMatrixTimesScalar %mat2v3float [[log2_c]] %float_0_30103
// CHECK-NEXT:                        OpStore %log10_c [[log10_c]]
  log10_c = log10(c);
}
