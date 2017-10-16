// Run: %dxc -T ps_6_0 -E main

void main() {

  float    a;
  float2   b;
  float2x3 c;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpFwidth %float [[a]]
  float    fwa = fwidth(a);

// CHECK:      [[b:%\d+]] = OpLoad %v2float %b
// CHECK-NEXT:   {{%\d+}} = OpFwidth %v2float [[b]]
  float2   fwb = fwidth(b);

// CHECK:        [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:  [[c0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[dc0:%\d+]] = OpFwidth %v3float [[c0]]
// CHECK-NEXT:  [[c1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[dc1:%\d+]] = OpFwidth %v3float [[c1]]
// CHECK-NEXT:     {{%\d+}} = OpCompositeConstruct %mat2v3float [[dc0]] [[dc1]]
  float2x3 fwc = fwidth(c);
}
