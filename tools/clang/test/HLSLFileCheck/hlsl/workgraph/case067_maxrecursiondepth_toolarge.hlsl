// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE067 (fail)
// recursive node with too many recursions max
// ==================================================================

struct RECURSIVE_RECORD
{
  uint value;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(256,1,1)]
[NumThreads(1024,1,1)]
[NodeMaxRecursionDepth(35)]
[NodeID("recursive_node")]
[NodeIsProgramEntry]
void node067a_maxrecursiondepth_toolarge(DispatchNodeInputRecord<RECURSIVE_RECORD> input,
                                        [MaxOutputRecords(2)][NodeID("recursive_node")] NodeOutput<RECURSIVE_RECORD> recursion,
                                        [MaxOutputRecordsSharedWith(recursion)][NodeID("target_node")] NodeOutput<RECURSIVE_RECORD> output)
{
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(256,1,1)]
[NumThreads(1024,1,1)]
[NodeID("target_node")]
void node067b_maxrecursiondepth_toolarge(DispatchNodeInputRecord<RECURSIVE_RECORD> input)
{
}

// CHECK: :16:2: error: NodeMaxRecursionDepth may not exceed 32
// CHECK-NEXT: [NodeMaxRecursionDepth(35)]
// CHECK-NEXT:  ^

