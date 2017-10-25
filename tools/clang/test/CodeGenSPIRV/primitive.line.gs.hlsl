// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main InputLines

[maxvertexcount(3)]
void main(line in uint id[2] : VertexID) {}
