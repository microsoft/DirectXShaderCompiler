// RUN: %dxc -T lib_6_9 %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 %s | FileCheck %s --check-prefixes=CHKCGL

// REQUIRES: dxil-1-9

// ==================================================================
// Test mesh nodes with various valid attribute combinations
// ==================================================================

[Shader("node")]
[NodeLaunch("mesh")]
[NodeDispatchGrid(1,2,3)]
[NumThreads(1,1,1)]
[OutputTopology("triangle")]
void node01_mesh_min_attribs() {}

// CHKCGL-DAG: !{void ()* @node01_mesh_min_attribs, i32 15, i32 1, i32 1, i32 1, i32 4, i1 false, !"node01_mesh_min_attribs", i32 0, !"", i32 0, i32 -1, i32 1, i32 2, i32 3, i32 0, i32 0, i32 0, i32 0, i32 2, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node01_mesh_min_attribs, !"node01_mesh_min_attribs", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID01:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 2,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 1, i32 2, i32 3}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 1, i32 1, i32 1}


[NodeIsProgramEntry]
[Shader("node")]
[OutputTopology("triangle")]
[NodeLaunch("mesh")]
[NodeDispatchGrid(2,3,2)]
[NumThreads(4,1,1)]
void node02_mesh_isprogramentry_attrib() {}

// CHKCGL-DAG: !{void ()* @node02_mesh_isprogramentry_attrib, i32 15, i32 4, i32 1, i32 1, i32 4, i1 true, !"node02_mesh_isprogramentry_attrib", i32 0, !"", i32 0, i32 -1, i32 2, i32 3, i32 2, i32 0, i32 0, i32 0, i32 0, i32 2, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node02_mesh_isprogramentry_attrib, !"node02_mesh_isprogramentry_attrib", null, null, [[ATTRS:![0-9]+]]}
//Metadata for isprogramentry node attributes
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeIsProgramEntry Tag (14)
// Arg #6: True (1)
// Arg #7: NodeId Tag (15)
// Arg #8: NodeId (NodeId metadata)
// Arg #9: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #10: Index (-1)
// Arg #11: NodeDispatchGrid Tag (18)
// Arg #12: NodeDispatchGrid (xyz metadata)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 14, i1 true, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 2,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 2, i32 3, i32 2}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 4, i32 1, i32 1}

[Shader("node")]
[OutputTopology("triangle")]
[NodeLaunch("mesh")]
[NodeID("new_node_name")]
[NumThreads(5,1,1)]
[NodeDispatchGrid(1,3,2)]
void node03_mesh_nodeid() {}

// CHKCGL-DAG: !{void ()* @node03_mesh_nodeid, i32 15, i32 5, i32 1, i32 1, i32 4, i1 false, !"new_node_name", i32 0, !"", i32 0, i32 -1, i32 1, i32 3, i32 2, i32 0, i32 0, i32 0, i32 0, i32 2, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node03_mesh_nodeid, !"node03_mesh_nodeid", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 2,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// NodeID
// Arg #1: NodeID = "new_node_name"
// Arg #2: Default Index (0)
// ------------------------------------------------------------------
// CHECK: [[NODEID]] = !{!"new_node_name", i32 0}

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 1, i32 3, i32 2}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 5, i32 1, i32 1}


[NumThreads(1,77,1)]
[Shader("node")]
[NodeDispatchGrid(1,3,9)]
[OutputTopology("triangle")]
[NodeLaunch("mesh")]
[NodeID("newish_node_name", 7)]
void node04_mesh_nodeid_idx() {}

// CHKCGL-DAG: !{void ()* @node04_mesh_nodeid_idx, i32 15, i32 1, i32 77, i32 1, i32 4, i1 false, !"newish_node_name", i32 7, !"", i32 0, i32 -1, i32 1, i32 3, i32 9, i32 0, i32 0, i32 0, i32 0, i32 2, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node04_mesh_nodeid_idx, !"node04_mesh_nodeid_idx", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// Arg #11: OutputTopology Tag (65536)
// Arg #12: triangle (2)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 2,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// NodeID
// Arg #1: NodeID = "newish_node_name"
// Arg #2: NodeID index 7
// ------------------------------------------------------------------
// CHECK: [[NODEID]] = !{!"newish_node_name", i32 7}

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 1, i32 3, i32 9}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 1, i32 77, i32 1}


[Shader("node")]
[NodeLaunch("mesh")]
[NodeLocalRootArgumentsTableIndex(7)]
[OutputTopology("line")]
[NodeDispatchGrid(7,2,3)]
[NumThreads(7,7,1)]
void node05_mesh_local_root_arguments_table_idx() {}

// CHKCGL-DAG: !{void ()* @node05_mesh_local_root_arguments_table_idx, i32 15, i32 7, i32 7, i32 1, i32 4, i1 false, !"node05_mesh_local_root_arguments_table_idx", i32 0, !"", i32 0, i32 7, i32 7, i32 2, i32 3, i32 0, i32 0, i32 0, i32 0, i32 1, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node05_mesh_local_root_arguments_table_idx, !"node05_mesh_local_root_arguments_table_idx", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (7)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// Arg #11: OutputTopology Tag (65536)
// Arg #12: line (1)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 7, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 1,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 7, i32 2, i32 3}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 7, i32 7, i32 1}

[NodeShareInputOf("node01_mesh_min_attribs")]
[Shader("node")]
[NodeLaunch("mesh")]
[NumThreads(1,2,64)]
[NodeDispatchGrid(1,1,128)]
[OutputTopology("line")]
void node06_mesh_share_input() {}

// CHKCGL-DAG: !{void ()* @node06_mesh_share_input, i32 15, i32 1, i32 2, i32 64, i32 4, i1 false, !"node06_mesh_share_input", i32 0, !"node01_mesh_min_attribs", i32 0, i32 -1, i32 1, i32 1, i32 128, i32 0, i32 0, i32 0, i32 0, i32 1, i32 0, i32 0, i32 0, i1 false}

// CHECK: = !{void ()* @node06_mesh_share_input, !"node06_mesh_share_input", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// Arg #11: OutputTopology Tag (65536)
// Arg #12: line (1)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 17, [[NODEID01]], i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 1,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 1, i32 1, i32 128}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 1, i32 2, i32 64}

struct RECORD {
  uint3 dg : SV_DispatchGrid;
};

[OutputTopology("line")]
[Shader("node")]
[NodeLaunch("mesh")]
[NumThreads(2,64,1)]
[NodeMaxDispatchGrid(27,11,1)]
void node07_mesh_max_dispatch_grid(DispatchNodeInputRecord<RECORD> input)
{
}

// CHKCGL-DAG: !{void (%"struct.DispatchNodeInputRecord<RECORD>"*)* @node07_mesh_max_dispatch_grid, i32 15, i32 2, i32 64, i32 1, i32 4, i1 false, !"node07_mesh_max_dispatch_grid", i32 0, !"", i32 0, i32 -1, i32 0, i32 0, i32 0, i32 27, i32 11, i32 1, i32 0, i32 1, i32 0, i32 0, i32 0, i1 false, i32 97, i32 0, i32 12, i32 0, i32 5, i32 3, i32 4}

// CHECK: = !{void ()* @node07_mesh_max_dispatch_grid, !"node07_mesh_max_dispatch_grid", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeMaxDispatchGrid Tag (22)
// Arg #10: NodeMaxDispatchGrid (xyz metadata)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID01:![0-9]+]], i32 16, i32 -1, i32 22, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 1,
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 27, i32 11, i32 1}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 2, i32 64, i32 1}

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(42,1,1)]
[NodeDispatchGrid(19,84,1)]
[NodeMaxInputRecordsPerGraphEntryRecord(11, false)]
void node08_mesh_max_input_records() {}

// CHKCGL-DAG: !{void ()* @node08_mesh_max_input_records, i32 15, i32 42, i32 1, i32 1, i32 4, i1 false, !"node08_mesh_max_input_records", i32 0, !"", i32 0, i32 -1, i32 19, i32 84, i32 1, i32 0, i32 0, i32 0, i32 0, i32 1, i32 0, i32 0, i32 11, i1 false}

// CHECK: = !{void ()* @node08_mesh_max_input_records, !"node08_mesh_max_input_records", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// Arg #11: OutputTopology Tag (65536)
// Arg #12: line (1)
// Arg #13: NodeMaxInputRecordsPerGraphEntryRecord Tag (65539)
// Arg #14: NodeMaxInputRecordsPerGraphEntryRecord (count,shared)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 1, i32 65539, [[MAXINPUT:![0-9]+]]
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 19, i32 84, i32 1}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 42, i32 1, i32 1}
// CHECK-DAG: [[MAXINPUT]] = !{i32 11, i1 false}

[Shader("node")]
[NodeLaunch("mesh")]
[NumThreads(122,1,1)]
[NodeDispatchGrid(17,76,1)]
[OutputTopology("line")]
[NodeMaxInputRecordsPerGraphEntryRecord(13, true)]
void node09_mesh_max_input_records_shared() {}

// CHKCGL-DAG: !{void ()* @node09_mesh_max_input_records_shared, i32 15, i32 122, i32 1, i32 1, i32 4, i1 false, !"node09_mesh_max_input_records_shared", i32 0, !"", i32 0, i32 -1, i32 17, i32 76, i32 1, i32 0, i32 0, i32 0, i32 0, i32 1, i32 0, i32 0, i32 13, i1 true}

// CHECK: = !{void ()* @node09_mesh_max_input_records_shared, !"node09_mesh_max_input_records_shared", null, null, [[ATTRS:![0-9]+]]}
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: Mesh (4)
// Arg #5: NodeId Tag (15)
// Arg #6: NodeId (NodeId metadata)
// Arg #7: NodeLocalRootArgumentsTableIndex Tag (16)
// Arg #8: Index (-1)
// Arg #9: NodeDispatchGrid Tag (18)
// Arg #10: NodeDispatchGrid (xyz metadata)
// Arg #11: OutputTopology Tag (65536)
// Arg #12: line (1)
// Arg #13: NodeMaxInputRecordsPerGraphEntryRecord Tag (655389)
// Arg #14: NodeMaxInputRecordsPerGraphEntryRecord (count,shared)
// ...
// Arg #n1: NumThreads Tag (4)
// Arg #n2: NumThreads (xyz metadata)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4, i32 15, [[NODEID:![0-9]+]], i32 16, i32 -1, i32 18, [[DISPATCHGRID:![0-9]+]], i32 65536, i32 1, i32 65539, [[MAXINPUT:![0-9]+]]
// CHECK-SAME: i32 4, [[NUMTHREADS:![0-9]+]]

// CHECK-SAME: }

// CHECK-DAG: [[DISPATCHGRID]] = !{i32 17, i32 76, i32 1}
// CHECK-DAG: [[NUMTHREADS]] = !{i32 122, i32 1, i32 1}
// CHECK-DAG: [[MAXINPUT]] = !{i32 13, i1 true}
