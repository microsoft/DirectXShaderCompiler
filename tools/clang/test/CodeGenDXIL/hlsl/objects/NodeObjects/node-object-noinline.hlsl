// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// XFAIL: *

[noinline]
void loadStressEmptyRecWorker(
EmptyNodeOutput outputNode)
{
	outputNode.GroupIncrementOutputCount(1);
}

[Shader("node")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void node_EmptyNodeOutput(
	[MaxOutputRecords(1)] EmptyNodeOutput loadStressChild
)
{
	loadStressEmptyRecWorker(wrapper(loadStressChild));
}
