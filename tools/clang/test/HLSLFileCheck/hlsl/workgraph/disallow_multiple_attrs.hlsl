// RUN: %dxc -T cs_6_8 -E node_compute %s | FileCheck %s

// CHECK: error: Conflicting shader profile
// CHECK: note: See conflicting shader attribute
// CHECK: error: Invalid shader stage attribute combination

[Shader("node")]
[Shader("compute")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(9,5,6)]
void node_compute() { }
