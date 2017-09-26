// Run: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"
// CHECK: [[v4f0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
// CHECK: [[v4f1:%\d+]] = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
// CHECK: [[v3f0:%\d+]] = OpConstantComposite %v3float %float_0 %float_0 %float_0
// CHECK: [[v3f1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1

void main() {
  float    a, sata;
  float4   b, satb;
  float2x3 c, satc;

// CHECK:         [[a:%\d+]] = OpLoad %float %a
// CHECK-NEXT: [[sata:%\d+]] = OpExtInst %float [[glsl]] FClamp [[a]] %float_0 %float_1
// CHECK-NEXT:                 OpStore %sata [[sata]]
  sata = saturate(a);

// CHECK:         [[b:%\d+]] = OpLoad %v4float %b
// CHECK-NEXT: [[satb:%\d+]] = OpExtInst %v4float [[glsl]] FClamp [[b]] [[v4f0]] [[v4f1]]
// CHECK-NEXT:                 OpStore %satb [[satb]]
  satb = saturate(b);

// CHECK:      [[c:%\d+]] = OpLoad %mat2v3float %c
// CHECK-NEXT: [[row0:%\d+]] = OpCompositeExtract %v3float [[c]] 0
// CHECK-NEXT: [[sat0:%\d+]] = OpExtInst %v3float [[glsl]] FClamp [[row0]] [[v3f0]] [[v3f1]]
// CHECK-NEXT: [[row1:%\d+]] = OpCompositeExtract %v3float [[c]] 1
// CHECK-NEXT: [[sat1:%\d+]] = OpExtInst %v3float [[glsl]] FClamp [[row1]] [[v3f0]] [[v3f1]]
// CHECK-NEXT: [[satc:%\d+]] = OpCompositeConstruct %mat2v3float [[sat0]] [[sat1]]
// CHECK-NEXT: OpStore %satc [[satc]]
  satc = saturate(c);
}
