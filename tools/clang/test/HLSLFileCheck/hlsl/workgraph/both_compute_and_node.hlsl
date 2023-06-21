// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// Test when permutations of compute and node are specified:
// - Check that Shader Kind is compute whenever compute is specified
// - Check that NodeId is present when and only when node is specified.
// - Check that NumThreads is present and correctly populated in all cases
// ==================================================================

[Shader("compute")]
[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(9,1,2)]
void compute_node() { }

// Both compute then node specified: Shader Kind = compute, NodeId present
// CHECK: !{void ()* @compute_node, !"compute_node", null, null, [[ATTRS_1:![0-9]+]]}
// CHECK: [[ATTRS_1]] = !{i32 8, i32 5, i32 13, i32 1,
// CHECK-SAME: i32 15, [[NODE_ID_1:![0-9]+]],
// CHECK-SAME: i32 4, [[NUM_THREADS_1:![0-9]+]],
// CHECK: [[NODE_ID_1]] = !{!"compute_node", i32 0}
// CHECK: [[NUM_THREADS_1]] = !{i32 9, i32 1, i32 2}

// ==================================================================

[Shader("compute")]
[NumThreads(9,3,4)]
void compute_only() { }

// Only compute specified: Shader Kind = compute, NodeId not present
// CHECK: !{void ()* @compute_only, !"compute_only", null, null, [[ATTRS_2:![0-9]+]]}
// CHECK: [[ATTRS_2]] =  !{i32 8, i32 5,
// CHECK-NOT: i32 15, {{![0-9]+}},
// CHECK-SAME: i32 4, [[NUM_THREADS_2:![0-9]+]],
// CHECK: [[NUM_THREADS_2]] = !{i32 9, i32 3, i32 4}

// ==================================================================

[Shader("node")]
[Shader("compute")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(9,5,6)]
void node_compute() { }

// Both node then compute specified: Shader Kind = compute, NodeId present
// CHECK: !{void ()* @node_compute, !"node_compute", null, null, [[ATTRS_3:![0-9]+]]}
// CHECK: [[ATTRS_3]] =  !{i32 8, i32 5,
// CHECK-SAME: i32 15, [[NODE_ID_3:![0-9]+]],
// CHECK-SAME: i32 4, [[NUM_THREADS_3:![0-9]+]],
// CHECK: [[NODE_ID_3]] = !{!"node_compute", i32 0}
// CHECK: [[NUM_THREADS_3]] = !{i32 9, i32 5, i32 6}

// ==================================================================

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(9,7,8)]
void node_only() { }

// Only node specified: Shader Kind = node, NodeId present
// CHECK: !{void ()* @node_only, !"node_only", null, null, [[ATTRS_4:![0-9]+]]}
// CHECK: [[ATTRS_4]] =  !{i32 8, i32 15,
// CHECK-SAME: i32 15, [[NODE_ID_4:![0-9]+]],
// CHECK-SAME: i32 4, [[NUM_THREADS_4:![0-9]+]],
// CHECK: [[NODE_ID_4]] = !{!"node_only", i32 0}
// CHECK: [[NUM_THREADS_4]] = !{i32 9, i32 7, i32 8}
