// Run: %dxc -T gs_6_0 -E main

[maxvertexcount(3)]
void main(triangle in uint i[3] : TriangleVertexID, line in uint j[2] : LineVertexID) {}

// CHECK: error: only one primitive type must be specified in the geometry shader