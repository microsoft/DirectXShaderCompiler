// RUN: %dxc -T ps_6_0 -E main

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpIsNan %bool [[a]]
  bool  isnan_a = isnan(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpIsNan %v4bool [[b]]
  bool4 isnan_b = isnan(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isnan_c = isnan(c);
}
