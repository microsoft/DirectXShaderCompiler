// RUN: %dxc -T lib_6_8 %s | FileCheck %s
//
// Verify that NodeOutput can be passed to a called function and used."

// RUN: %dxc -fcgl -T lib_6_8 %s | FileCheck -check-prefix=CHECK_FCGL %s
//
// Verify that the correct parameter, the NodeOutputRecord parameter, gets the cast
// CHECK_FCGL: %[[outputNodeVal:[0-9]+]] = load %"struct.NodeOutput<loadStressRecord>", %"struct.NodeOutput<loadStressRecord>"* %outputNode,
// CHECK_FCGL: %[[outputNodeHandle:[0-9]+]] = call %dx.types.NodeHandle @"dx.hl.cast..%dx.types.NodeHandle (i32, %\22struct.NodeOutput<loadStressRecord>\22)"(i32 10, %"struct.NodeOutput<loadStressRecord>" %[[outputNodeVal]])
// outputNode.GetGroupNodeOutputRecords(13):
// CHECK_FCGL: call %dx.types.NodeRecordHandle @"dx.hl.op..%dx.types.NodeRecordHandle (i32, %dx.types.NodeHandle, i32)"(i32 {{[0-9]+}}, %dx.types.NodeHandle %[[outputNodeHandle]], i32 13)

struct loadStressRecord
{
    uint3 grid : SV_DispatchGrid;
};

void loadStressWorker(
    NodeOutput<loadStressRecord> outputNode)
{
    GroupNodeOutputRecords<loadStressRecord> outRec = outputNode.GetGroupNodeOutputRecords(13);    
    // CHECK: store i32 39
    // CHECK: store i32 61
    // CHECK: store i32 71
    outRec.Get(5).grid = uint3(39, 61, 71);
}

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void loadStress_16(    
    [MaxRecords(16)] NodeOutput<loadStressRecord> loadStressChild
)
{
    loadStressWorker(loadStressChild);
}
