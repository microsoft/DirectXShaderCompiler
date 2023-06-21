// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE052 (fail)
// Invalid NodeLaunch value
// ==================================================================

struct INPUT_NOGRID
{
  uint textureIndex;
};

[Shader("node")]
[NodeLaunch("Other")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node052_nodelaunch_invalid(DispatchNodeInputRecord<INPUT_NOGRID> input)
{
}

// CHECK: 13:13: error: attribute 'NodeLaunch' must have one of these values: broadcasting,coalescing,thread
