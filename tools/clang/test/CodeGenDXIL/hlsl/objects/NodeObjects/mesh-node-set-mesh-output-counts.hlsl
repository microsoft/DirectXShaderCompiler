// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// REQUIRES: dxil-1-9
 
// Ensure that setMeshOutputCounts can be correctly lowered in a mesh node

// CHECK: define void @node_setmeshoutputcounts()
// CHECK: dx.op.setMeshOutputCounts(i32 168, i32 32, i32 16)
// CHECK: ret void

// CHECK: declare void @dx.op.setMeshOutputCounts(i32, i32, i32) #0

RWBuffer<float> buf0;


[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void node_setmeshoutputcounts() {
    SetMeshOutputCounts(32, 16);
    buf0[0] = 1.0;
}
