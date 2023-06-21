// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE007
// Broadcasting launch node with NumThreads not specified

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2,3,2)]
[NodeIsProgramEntry]
void node007_broadcasting_numthreads_none()
{
}

// CHECK: :10:6: error: NumThreads is required, but was not specified
// CHECK-NEXT: void node007_broadcasting_numthreads_none()
// CHECK-NEXT:      ^
