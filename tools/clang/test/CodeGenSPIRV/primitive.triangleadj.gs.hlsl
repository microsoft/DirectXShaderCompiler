// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main InputTrianglesAdjacency

[maxvertexcount(3)]
void main(triangleadj in uint id[6] : VertexID) {}
