// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability DerivativeControl

void main() {

  float  a;
  float4 b;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpDPdxCoarse %float [[a]]
  float    da = ddx_coarse(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpDPdxCoarse %v4float [[b]]
  float4   db = ddx_coarse(b);
}
