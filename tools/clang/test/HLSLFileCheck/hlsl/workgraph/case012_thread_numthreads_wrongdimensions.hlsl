// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE012 (fail)
// Thread launch node with incompatible thread group dimensions
// ==================================================================

[Shader("node")]
[NodeLaunch("Thread")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node012_thread_numthreads_wrongdimensions()
{
}

// CHECK: :9:2: error: Thread launch nodes must have a thread group size of (1,1,1)
// CHECK-NEXT: [NumThreads(1024,1,1)]
// CHECK-NEXT:  ^
// CHECK: :8:2: note: Launch type defined here
// CHECK-NEXT: [NodeLaunch("Thread")]
// CHECK-NEXT:  ^

