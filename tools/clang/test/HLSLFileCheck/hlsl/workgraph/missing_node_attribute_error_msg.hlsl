// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// check that an error gets generated each time a "node" attribute gets used without the presence of 
// the '[shader("node")]' attribute

[Shader("compute")]
[NodeIsProgramEntry]
[NodeID("nodeName")]
[NodeLocalRootArgumentsTableIndex(3)]
[NodeShareInputOf("nodeName")]
[NodeMaxRecursionDepth(31)]
[NodeDispatchGrid(1, 1, 1)]
[NodeMaxDispatchGrid(2,2,2)]
[NumThreads(1, 1, 1)]
void secondNode()
{
    // CHECK-DAG: attribute NodeIsProgramEntry only allowed on node shaders
    // CHECK-DAG: attribute NodeId only allowed on node shaders
    // CHECK-DAG: attribute NodeLocalRootArgumentsTableIndex only allowed on node shaders
    // CHECK-DAG: attribute NodeShareInputOf only allowed on node shaders
    // CHECK-DAG: attribute NodeMaxRecursionDepth only allowed on node shaders
    // CHECK-DAG: attribute NodeDispatchGrid only allowed on node shaders
    // CHECK-DAG: attribute NodeMaxDispatchGrid only allowed on node shaders
}
