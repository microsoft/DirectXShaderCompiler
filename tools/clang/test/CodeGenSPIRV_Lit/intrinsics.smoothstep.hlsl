// RUN: %dxc -T ps_6_0 -E main

// CHECK:  [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float min, max, val;
  float4 min4, max4, val4;
  float2x3 min2x3, max2x3, val2x3;

// CHECK:      [[min:%\d+]] = OpLoad %float %min
// CHECK-NEXT: [[max:%\d+]] = OpLoad %float %max
// CHECK-NEXT: [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT:     {{%\d+}} = OpExtInst %float [[glsl]] SmoothStep [[min]] [[max]] [[val]]
  float       ss = smoothstep(min, max, val);

// CHECK:      [[min4:%\d+]] = OpLoad %v4float %min4
// CHECK-NEXT: [[max4:%\d+]] = OpLoad %v4float %max4
// CHECK-NEXT: [[val4:%\d+]] = OpLoad %v4float %val4
// CHECK-NEXT:      {{%\d+}} = OpExtInst %v4float [[glsl]] SmoothStep [[min4]] [[max4]] [[val4]]
  float4     ss4 = smoothstep(min4, max4, val4);

// CHECK:      [[min2x3:%\d+]] = OpLoad %mat2v3float %min2x3
// CHECK-NEXT: [[max2x3:%\d+]] = OpLoad %mat2v3float %max2x3
// CHECK-NEXT: [[val2x3:%\d+]] = OpLoad %mat2v3float %val2x3
// CHECK-NEXT: [[min_r0:%\d+]] = OpCompositeExtract %v3float [[min2x3]] 0
// CHECK-NEXT: [[max_r0:%\d+]] = OpCompositeExtract %v3float [[max2x3]] 0
// CHECK-NEXT: [[val_r0:%\d+]] = OpCompositeExtract %v3float [[val2x3]] 0
// CHECK-NEXT:  [[ss_r0:%\d+]] = OpExtInst %v3float [[glsl]] SmoothStep [[min_r0]] [[max_r0]] [[val_r0]]
// CHECK-NEXT: [[min_r1:%\d+]] = OpCompositeExtract %v3float [[min2x3]] 1
// CHECK-NEXT: [[max_r1:%\d+]] = OpCompositeExtract %v3float [[max2x3]] 1
// CHECK-NEXT: [[val_r1:%\d+]] = OpCompositeExtract %v3float [[val2x3]] 1
// CHECK-NEXT:  [[ss_r1:%\d+]] = OpExtInst %v3float [[glsl]] SmoothStep [[min_r1]] [[max_r1]] [[val_r1]]
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %mat2v3float [[ss_r0]] [[ss_r1]]
  float2x3 ss2x3 = smoothstep(min2x3, max2x3, val2x3);
}
