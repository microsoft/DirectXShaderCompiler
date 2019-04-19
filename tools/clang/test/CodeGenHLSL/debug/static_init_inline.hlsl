// RUN: %dxc -E main -T ps_6_0 %s -Zi -Od | FileCheck %s

// Make sure when there is non-trivial global variable initialization, the
// inlined initialization instructions have "inlinedAt" property

// CHECK: !{{[0-9]+}} = !DILocation(line: {{[0-9]+}}, column: {{[0-9]+}}, scope: !{{[0-9]+}}, inlinedAt:

static float4 my_value = float4(1,2,3,4);
static float4 my_value2 = my_value*2;

[RootSignature("")]
float4 main() : SV_Target {
  return my_value2;
}
