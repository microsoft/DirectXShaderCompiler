// Run: %dxc -T ps_6_0 -E main

void main() {

  float    a;
  float2   b;
  float2x3 c;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpDPdy %float [[a]]
  float    da = ddy(a);


// CHECK:      [[b:%\d+]] = OpLoad %v2float %b
// CHECK-NEXT:   {{%\d+}} = OpDPdy %v2float [[b]]
  float2   db = ddy(b);

// CHECK:        [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:  [[c0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[dc0:%\d+]] = OpDPdy %v3float [[c0]]
// CHECK-NEXT:  [[c1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[dc1:%\d+]] = OpDPdy %v3float [[c1]]
// CHECK-NEXT:     {{%\d+}} = OpCompositeConstruct %mat2v3float [[dc0]] [[dc1]]
  float2x3 dc = ddy(c);
}
