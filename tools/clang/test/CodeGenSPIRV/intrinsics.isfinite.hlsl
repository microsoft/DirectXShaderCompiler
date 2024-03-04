// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// Since OpIsFinite needs the Kernel capability, translation is done using OpIsNan and OpIsInf.
// isFinite = !(isNan || isInf)

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:               [[a:%[0-9]+]] = OpLoad %float %a
// CHECK-NEXT:    [[a_isNan:%[0-9]+]] = OpIsNan %bool [[a]]
// CHECK-NEXT:    [[a_isInf:%[0-9]+]] = OpIsInf %bool [[a]]
// CHECK-NEXT: [[a_NanOrInf:%[0-9]+]] = OpLogicalOr %bool [[a_isNan]] [[a_isInf]]
// CHECK-NEXT:            {{%[0-9]+}} = OpLogicalNot %bool [[a_NanOrInf]]
  bool    isf_a = isfinite(a);

// CHECK:                [[b:%[0-9]+]] = OpLoad %v4float %b
// CHECK-NEXT:     [[b_isNan:%[0-9]+]] = OpIsNan %v4bool [[b]]
// CHECK-NEXT:     [[b_isInf:%[0-9]+]] = OpIsInf %v4bool [[b]]
// CHECK-NEXT:  [[b_NanOrInf:%[0-9]+]] = OpLogicalOr %v4bool [[b_isNan]] [[b_isInf]]
// CHECK-NEXT:             {{%[0-9]+}} = OpLogicalNot %v4bool [[b_NanOrInf]]
  bool4   isf_b = isfinite(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isf_c = isfinite(c);
}
