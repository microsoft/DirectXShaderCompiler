// RUN: %dxc -E main -T ps_6_1 %s | FileCheck %s
// CHECK: Number of inputs: 4, outputs: 4
// CHECK: output 0 depends on inputs: { 0, 1 }
// CHECK-NEXT: {{; *}}output 1 depends on inputs: { 2 }
// CHECK-NOT: output 2 depends
// CHECK-NOT: output 3 depends

[noinline] void add(float value, inout float result) { result += value; }
[noinline] void forward(float value, inout float result) { add(value, result); }
[noinline] void conditional(float condition, out float result) {
  result = condition > 0 ? 3 : 5;
}
float4 main(float4 input : INPUT) : SV_Target {
  float a = input.x;
  forward(input.y, a);
  float b;
  conditional(input.z, b);
  return float4(a, b, 7, 11);
}
