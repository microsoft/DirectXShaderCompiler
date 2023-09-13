// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability DerivativeControl

void main() {

  float  a;
  float4 b;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpDPdyFine %float [[a]]
  float    da = ddy_fine(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpDPdyFine %v4float [[b]]
  float4   db = ddy_fine(b);
}
