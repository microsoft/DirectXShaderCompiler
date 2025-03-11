// RUN: %dxc -T lib_6_9 -Wno-hlsl-require-max-records-per-node %s | FileCheck %s

// Tests for overrriding [MaxRecordsPerNode] attribute requirement with -Wno
// Test MaxRecordsPerNode set to MaxRecords

struct RECORD1
{
    uint value;
    uint value2;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(128, 1, 1)]
void node_1_0(
    [NodeArraySize(128)] [MaxRecords(64)] NodeOutputArray<RECORD1> OutputArray
)
{
}

// CHECK-NOT: error: MaxRecordsPerNode is a required attribute SM6.9+ [-Whlsl-require-max-records-per-node]
// CHECK: !{void ()* @node_1_0, !"node_1_0", null, null, [[NODE_1_0:![0-9]+]]}
// CHECK: [[NODE_1_0]] = !{i32 8, i32 15, i32 13, i32 1, i32 15, !10, i32 16, i32 -1, i32 18, !11, i32 21, [[OUTPUTS:![0-9]+]]
// CHECK: [[OUTPUTS]] = !{[[OUTPUT1:![0-9]+]]}
// CHECK: [[OUTPUT1]] = !{i32 1, i32 22, i32 2, !14, i32 3, i32 [[MAXRECORDS:[0-9]+]], i32 5, i32 128, i32 0, !15, i32 7, i32 [[MAXRECORDS]]}

