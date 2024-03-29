// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: max primitive count should not exceed 256

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("line")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out indices uint3 primIndices[257]) { }
