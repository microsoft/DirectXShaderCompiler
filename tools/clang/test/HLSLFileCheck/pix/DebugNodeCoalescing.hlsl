// RUN: %dxc -T lib_6_8 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=3,parameter1=1,parameter2=0 -hlsl-dxilemit | %FileCheck %s

// A coalescing node has no dispatch grid, so dx.op.threadId is not legal for it.
// ValidateDxilOperationCallInProfile in DxilValidation.cpp permits ThreadId and
// GroupId for a broadcasting launch only. The thread group ID is legal, so the
// debugger discriminates invocations within a group by SV_GroupThreadID.

// CHECK: NodeInvocationSelection:GroupThreadId

// CHECK: %ThreadIdX = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)
// CHECK: %ThreadIdY = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)
// CHECK: %ThreadIdZ = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 2)
// CHECK: %CompareToThreadIdX = icmp eq i32 %ThreadIdX, 3
// CHECK: %CompareToThreadIdY = icmp eq i32 %ThreadIdY, 1
// CHECK: %CompareToThreadIdZ = icmp eq i32 %ThreadIdZ, 0
// CHECK: %CompareAll = and i1 %CompareXAndY, %CompareToThreadIdZ
// CHECK: br i1 %CompareAll, label %PIXInterestingBlock, label %PIXNonInterestingBlock

// The requested thread must lie inside the declared thread group, or the pass
// discriminates nothing. The parameters above lie inside [NumThreads(4, 2, 1)].
// CHECK-NOT: NodeInvocationSelection:None

RWStructuredBuffer<uint> Output : register(u0);

struct Record
{
  uint value;
};

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(4, 2, 1)]
void CoalescingNode(GroupNodeInputRecords<Record> input)
{
  Output[0] = input.Get(0).value;
}
