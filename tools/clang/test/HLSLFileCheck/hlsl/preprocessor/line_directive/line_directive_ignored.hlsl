// RUN: %dxc -E main -T ps_6_0 -ignore-line-directives 2> %s | FileCheck %s

// #line directive should be ignored due to -ignore-line-directives
// CHECK-NOT: renamed.hlsl:42

float4 main() : SV_Target
{
#line 42 "renamed.hlsl"
  return foo; // Undeclared identifier
}