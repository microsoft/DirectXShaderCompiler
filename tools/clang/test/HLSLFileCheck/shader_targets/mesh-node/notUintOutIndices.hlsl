// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: the element of out_indices array must be uint2 for line output or uint3 for triangle output

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("line")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out indices float3 primIndices[16]) { }
