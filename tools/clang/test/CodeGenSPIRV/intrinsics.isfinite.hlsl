// Run: %dxc -T ps_6_0 -E main

// Since OpIsFinite needs the Kernel capability, translation is done using OpIsNan and OpIsInf.
// isFinite = !(isNan || isInf)

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:               [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:    [[a_isNan:%\d+]] = OpIsNan %bool [[a]]
// CHECK-NEXT:    [[a_isInf:%\d+]] = OpIsInf %bool [[a]]
// CHECK-NEXT: [[a_NanOrInf:%\d+]] = OpLogicalOr %bool [[a_isNan]] [[a_isInf]]
// CHECK-NEXT:            {{%\d+}} = OpLogicalNot %bool [[a_NanOrInf]]
  bool    isf_a = isfinite(a);

// CHECK:                [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:     [[b_isNan:%\d+]] = OpIsNan %v4bool [[b]]
// CHECK-NEXT:     [[b_isInf:%\d+]] = OpIsInf %v4bool [[b]]
// CHECK-NEXT:  [[b_NanOrInf:%\d+]] = OpLogicalOr %v4bool [[b_isNan]] [[b_isInf]]
// CHECK-NEXT:             {{%\d+}} = OpLogicalNot %v4bool [[b_NanOrInf]]
  bool4   isf_b = isfinite(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isf_c = isfinite(c);
}
