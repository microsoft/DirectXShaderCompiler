// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: primitives output is not an constant-length array

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out primitives uint3 primIndices) {
  primIndices = 1;
}