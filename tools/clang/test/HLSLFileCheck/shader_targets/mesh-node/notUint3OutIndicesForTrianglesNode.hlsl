// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: the element of out_indices array in a mesh shader whose output topology is triangle must be uint3

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out indices uint2 primIndices[16]) { }
