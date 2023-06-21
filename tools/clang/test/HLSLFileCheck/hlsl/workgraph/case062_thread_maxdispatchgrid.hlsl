// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE062 (fail)
// Thread launch node declared max dispatch grid
// ==================================================================

struct INPUT_RECORD
{
  uint3 DispatchGrid : SV_DipatchGrid;
  uint foo;
};

[Shader("node")]
[NodeLaunch("Thread")]
[NodeMaxDispatchGrid(2,3,2)]
[NodeIsProgramEntry]
void node062_thread_maxdispatchgrid(DispatchNodeInputRecord<INPUT_RECORD> input)
{
}

// CHECK: :15:2: error: NodeMaxDispatchGrid may only be used with Broadcasting nodes
// CHECK-NEXT: [NodeMaxDispatchGrid(2,3,2)]
// CHECK-NEXT:  ^

