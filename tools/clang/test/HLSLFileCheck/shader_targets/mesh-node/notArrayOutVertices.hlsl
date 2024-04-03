// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: vertices output is not an constant-length array

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out vertices uint3 verts) {
  verts = 1;
}
