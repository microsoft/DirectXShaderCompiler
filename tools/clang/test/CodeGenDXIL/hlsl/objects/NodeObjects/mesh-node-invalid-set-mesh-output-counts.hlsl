// RUN: not %dxc -T lib_6_9 %s 2>&1 | FileCheck %s

// REQUIRES: dxil-1-9
 
// Ensure that setMeshOutputCounts will be appropriately rejected by non-mesh nodes

RWBuffer<float> buf0;

// CHECK:  14: error: Function uses features incompatible with the shader stage (node) of the entry function.
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeDispatchGrid(64,1,1)]
[NodeLaunch("broadcasting")]
void node_broadcasting() {
  SetMeshOutputCounts(32, 16);
  buf0[0] = 1.0;
}

// CHECK:  23: error: Function uses features incompatible with the shader stage (node) of the entry function.
[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
void node_coalescing() {
  SetMeshOutputCounts(32, 16);
  buf0[0] = 2.0;
}

// CHECK:  31: error: Function uses features incompatible with the shader stage (node) of the entry function.
[Shader("node")]
[NodeLaunch("thread")]
void node_thread() {
  SetMeshOutputCounts(32, 16);
  buf0[0] = 4.0;
}
