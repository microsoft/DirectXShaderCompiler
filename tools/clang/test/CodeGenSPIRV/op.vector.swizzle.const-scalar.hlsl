// RUN: %dxc -T ps_6_0 -E main

// CHECK:  [[v4f1:%\d+]] = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
// CHECK: [[v4f25:%\d+]] = OpConstantComposite %v4float %float_2_5 %float_2_5 %float_2_5 %float_2_5
// CHECK:  [[v4f0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
// CHECK:  [[v3f1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1

float4 main(float input: INPUT) : SV_Target {

// CHECK: OpStore %a [[v4f1]]
  float4 a = (1).xxxx;

// CHECK: OpStore %b [[v4f25]]
  float4 b = (2.5).xxxx;

// CHECK: OpStore %c [[v4f0]]
  float4 c = (false).xxxx;

// CHECK:       [[cc:%\d+]] = OpCompositeConstruct %v2float %float_0 %float_0
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeExtract %float [[cc]] 0
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeExtract %float [[cc]] 1
// CHECK-NEXT: [[rhs:%\d+]] = OpCompositeConstruct %v4float {{%\d+}} {{%\d+}} [[cc0]] [[cc1]]
// CHECK-NEXT:                OpStore %d [[rhs]]
  float4 d = float4(input, input, 0.0.xx);

// CHECK:        [[e:%\d+]] = OpLoad %v4float %e
// CHECK-NEXT: [[val:%\d+]] = OpVectorShuffle %v4float [[e]] [[v3f1]] 4 5 6 3
// CHECK-NEXT:                OpStore %e [[val]]
  float4 e;
  (e.xyz)  = 1.0.xxx; // Parentheses

// CHECK: OpReturnValue [[v4f0]]
  return 0.0.xxxx;
}
