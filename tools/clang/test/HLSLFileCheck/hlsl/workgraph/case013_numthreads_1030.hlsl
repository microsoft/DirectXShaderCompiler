// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE013 (fail)
// Broadcasting launch node with > 1024 threads in a group
// ==================================================================

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2,3,2)]
[NumThreads(1030,1,1)]
[NodeIsProgramEntry]
void node013_numthreads_1030()
{
}

// CHECK: case013_numthreads_1030.hlsl:10:2: error: Thread group size may not exceed 1024
// CHECK: [NumThreads(1030,1,1)]
// CHECK:  ^

