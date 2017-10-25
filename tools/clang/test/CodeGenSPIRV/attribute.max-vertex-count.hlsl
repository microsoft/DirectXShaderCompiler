// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main OutputVertices 3

[maxvertexcount(3)]
void main() {}
