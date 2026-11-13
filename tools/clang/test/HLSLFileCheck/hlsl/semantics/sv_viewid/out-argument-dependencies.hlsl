// RUN: %dxc -E main -T ps_6_1 %s | FileCheck %s
// CHECK: Number of inputs: 4, outputs: 4
// CHECK: Outputs dependent on ViewId: {  }
// CHECK: output 0 depends on inputs: { 0 }
// CHECK-NEXT: {{; *}}output 1 depends on inputs: { 1 }
// CHECK-NOT: output 2 depends
// CHECK-NOT: output 3 depends

[noinline] void copyValue(float value, out float result) { result = value; }
[noinline] void constantValue(float ignored, out float result) { result = 7; }
[noinline] void dead(float value, out float result) { result = value; }
float4 main(float4 input : INPUT) : SV_Target {
  float a, b, c, unrelated;
  copyValue(input.x, a);
  copyValue(input.y, b);
  constantValue(input.z, c);
  copyValue(input.w, unrelated);
  return float4(a, b, c, 11);
}
