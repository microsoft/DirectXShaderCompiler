// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability DerivativeControl

void main() {

  float  a;
  float4 b;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpDPdyCoarse %float [[a]]
  float    da = ddy_coarse(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpDPdyCoarse %v4float [[b]]
  float4   db = ddy_coarse(b);
}
