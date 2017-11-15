// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main Invocations 1

[maxvertexcount(2)]
void main() {}
