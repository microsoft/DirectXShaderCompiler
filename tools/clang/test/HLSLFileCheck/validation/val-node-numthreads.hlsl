// RUN: %dxc -T lib_6_8 %s | FileCheck %s

// CHECK: !{void ()* @compute_node, !"compute_node", null, null, ![[PROPS:[0-9]+]]}
// CHECK: ![[PROPS]] = !{i32 8, i32 5,
// CHECK-SAME: i32 4, ![[THREADDIM:[0-9]+]]
// CHECK: ![[THREADDIM]] = !{i32 9, i32 1, i32 2}

[Shader("compute")]
[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(9,1,2)]
void compute_node() { }
