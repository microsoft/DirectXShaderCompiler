// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main Invocations 42

[maxvertexcount(2)]
[instance(42)]
void main() {}
