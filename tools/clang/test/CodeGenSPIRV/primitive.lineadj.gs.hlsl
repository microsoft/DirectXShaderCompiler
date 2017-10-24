// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main InputLinesAdjacency

[maxvertexcount(3)]
void main(lineadj in uint id[4] : VertexID) {}
