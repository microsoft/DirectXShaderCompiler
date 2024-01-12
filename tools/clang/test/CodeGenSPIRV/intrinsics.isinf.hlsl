// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

void main() {
  float    a;
  float4   b;
  float2x3 c;

// CHECK:      [[a:%[0-9]+]] = OpLoad %float %a
// CHECK-NEXT:   {{%[0-9]+}} = OpIsInf %bool [[a]]
  bool  isinf_a = isinf(a);

// CHECK:      [[b:%[0-9]+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%[0-9]+}} = OpIsInf %v4bool [[b]]
  bool4 isinf_b = isinf(b);

  // TODO: We can not translate the following since boolean matrices are currently not supported.
  // bool2x3 isinf_c = isinf(c);
}
