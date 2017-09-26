// Run: %dxc -T ps_6_0 -E main

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpIsInf %bool [[a]]
  bool  isinf_a = isinf(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpIsInf %v4bool [[b]]
  bool4 isinf_b = isinf(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isinf_c = isinf(c);
}
