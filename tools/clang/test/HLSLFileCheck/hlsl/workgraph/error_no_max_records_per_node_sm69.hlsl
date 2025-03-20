// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// Test for required attribute 

struct RECORD1
{
    uint value;
    uint value2;
};
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(128, 1, 1)]
void node_1_2(
    [NodeArraySize(128)] [MaxRecords(64)] NodeOutputArray<RECORD1> OutputArray
)
{
}
// CHECK: error: MaxRecordsPerNode is a required attribute SM6.9+