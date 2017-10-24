// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main InputPoints

[maxvertexcount(3)]
void main(point in uint id[1] : VertexID) {}
