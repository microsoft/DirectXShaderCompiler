// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:      [[a:%[0-9]+]] = OpLoad %float %a
// CHECK-NEXT:   {{%[0-9]+}} = OpIsNan %bool [[a]]
  bool  isnan_a = isnan(a);

// CHECK:      [[b:%[0-9]+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%[0-9]+}} = OpIsNan %v4bool [[b]]
  bool4 isnan_b = isnan(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isnan_c = isnan(c);
}
