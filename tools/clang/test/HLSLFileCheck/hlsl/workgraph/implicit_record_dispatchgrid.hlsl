// RUN: %dxc -T lib_6_8 %s | FileCheck %s

// The purpose of this test is to verify the behavior of the compiler on node
// shaders with no input records

// CHECK: error: node shader 'cs_and_node' with NodeMaxDispatchGrid attribute must have an input node with the SV_DispatchGrid semantic

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void cs_and_node()
{
}
