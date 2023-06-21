// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE059 (fail)
// Thread launch node declared dispatch grid
// ==================================================================

[Shader("node")]
[NodeLaunch("Thread")]
[NodeDispatchGrid(2,3,2)]
[NodeIsProgramEntry]
void node059_thread_dispatchgrid()
{
}

// CHECK: error: NodeDispatchGrid may only be used with Broadcasting nodes
// CHECK-NEXT: [NodeDispatchGrid(2,3,2)]
// CHECK-NEXT:  ^
