// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE061 (fail)
// Coalescing launch node declared max dispatch grid
// ==================================================================

struct INPUT_RECORD
{
  uint3 DispatchGrid : SV_DipatchGrid;
  uint foo;
};

[Shader("node")]
[NodeLaunch("Coalescing")]
[NodeMaxDispatchGrid(2,3,2)]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node061_coalescing_maxdispatchgrid(DispatchNodeInputRecord<INPUT_RECORD> input)
{
}

// CHECK: 15:2: error: NodeMaxDispatchGrid may only be used with Broadcasting nodes
// CHECK-NEXT: [NodeMaxDispatchGrid(2,3,2)]
// CHECK-NEXT:  ^