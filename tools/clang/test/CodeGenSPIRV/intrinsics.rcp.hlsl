// Run: %dxc -T vs_6_0 -E main

// CHECK: [[v4f1:%\d+]] = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
// CHECK: [[v3f1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1
// CHECK: [[v4d1:%\d+]] = OpConstantComposite %v4double %double_1 %double_1 %double_1 %double_1
// CHECK: [[v3d1:%\d+]] = OpConstantComposite %v3double %double_1 %double_1 %double_1

void main() {
  float    a, rcpa;
  float4   b, rcpb;
  float2x3 c, rcpc;
  
  double    d, rcpd;
  double4   e, rcpe;
  double2x3 f, rcpf;

// CHECK:      [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT:   {{%\d+}} = OpFDiv %float %float_1 [[a]]
  rcpa = rcp(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT:   {{%\d+}} = OpFDiv %v4float [[v4f1]] [[b]]
  rcpb = rcp(b);

// CHECK:          [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT:    [[c0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[rcpc0:%\d+]] = OpFDiv %v3float [[v3f1]] [[c0]]
// CHECK-NEXT:    [[c1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[rcpc1:%\d+]] = OpFDiv %v3float [[v3f1]] [[c1]]
// CHECK-NEXT:       {{%\d+}} = OpCompositeConstruct %mat2v3float [[rcpc0]] [[rcpc1]]
  rcpc = rcp(c);

// CHECK:      [[d:%\d+]] = OpLoad %double %d
// CHECK-NEXT:   {{%\d+}} = OpFDiv %double %double_1 [[d]]
  rcpd = rcp(d);  

// CHECK:    [[e:%\d+]] = OpLoad %v4double %e
// CHECK-NEXT: {{%\d+}} = OpFDiv %v4double [[v4d1]] [[e]]
  rcpe = rcp(e);

// CHECK:          [[f:%\d+]] = OpLoad %mat2v3double %f
// CHECK-NEXT:    [[f0:%\d+]] = OpCompositeExtract %v3double [[f]] 0
// CHECK-NEXT: [[rcpf0:%\d+]] = OpFDiv %v3double [[v3d1]] [[f0]]
// CHECK-NEXT:    [[f1:%\d+]] = OpCompositeExtract %v3double [[f]] 1
// CHECK-NEXT: [[rcpf1:%\d+]] = OpFDiv %v3double [[v3d1]] [[f1]]
// CHECK-NEXT:       {{%\d+}} = OpCompositeConstruct %mat2v3double [[rcpf0]] [[rcpf1]]
  rcpf = rcp(f);
}
