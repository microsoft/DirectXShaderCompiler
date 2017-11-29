// Run: %dxc -T ps_6_0 -E main

// CHECK:  [[v4f1:%\d+]] = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
// CHECK: [[v4f25:%\d+]] = OpConstantComposite %v4float %float_2_5 %float_2_5 %float_2_5 %float_2_5
// CHECK:  [[v4f0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0

void main() {

// CHECK: OpStore %a [[v4f1]]
  float4 a = (1).xxxx;

// CHECK: OpStore %b [[v4f25]]
  float4 b = (2.5).xxxx;

// CHECK: OpStore %c [[v4f0]]
  float4 c = (false).xxxx;
}
