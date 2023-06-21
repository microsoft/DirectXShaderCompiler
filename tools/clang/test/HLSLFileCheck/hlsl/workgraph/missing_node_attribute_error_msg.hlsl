// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// check that an error gets generated each time a "node" attribute gets used without the presence of 
// the '[shader("node")]' attribute

[Shader("compute")]
[NodeIsProgramEntry]
[NodeID("nodeName")]
[NodeLocalRootArgumentsTableIndex(3)]
[NodeShareInputOf("nodeName")]
[NodeMaxRecursionDepth(51)]
[NodeDispatchGrid(1, 1, 1)]
[NodeMaxDispatchGrid(2,2,2)]
[NumThreads(1, 1, 1)]
void secondNode()
{
    // CHECK-DAG: Attribute nodeisprogramentry only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodeid only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodelocalrootargumentstableindex only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodeshareinputof only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodemaxrecursiondepth only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodedispatchgrid only applies to node shaders (indicated with '[shader("node")]')
    // CHECK-DAG: Attribute nodemaxdispatchgrid only applies to node shaders (indicated with '[shader("node")]')
}