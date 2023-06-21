// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE058 (fail)
// Coalescing launch node declared dispatch grid
// ==================================================================

[Shader("node")]
[NodeLaunch("Coalescing")]
[NodeDispatchGrid(2,3,2)]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node058_coalescing_dispatchgrid()
{
}

// CHECK: error: NodeDispatchGrid may only be used with Broadcasting nodes
// CHECK-NEXT: [NodeDispatchGrid(2,3,2)]
// CHECK-NEXT:  ^
