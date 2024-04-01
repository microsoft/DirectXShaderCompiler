// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: the element of out_indices array in a mesh shader whose output topology is line must be uint2

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("line")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out indices uint3 primIndices[16]) { }
