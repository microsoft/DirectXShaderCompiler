// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE009
// Coalescing launch node with NumThreads not specified

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2,3,2)]
[NodeIsProgramEntry]
void node009_coalescing_numthreads_none()
{
}

// CHECK: :10:6: error: NumThreads is required, but was not specified
// CHECK-NEXT: void node009_coalescing_numthreads_none()
// CHECK-NEXT:      ^
