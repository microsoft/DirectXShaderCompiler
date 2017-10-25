// Run: %dxc -T gs_6_0 -E main

// CHECK: OpExecutionMode %main Triangles

[maxvertexcount(3)]
void main(triangle in uint i[3] : VertexID) {}
