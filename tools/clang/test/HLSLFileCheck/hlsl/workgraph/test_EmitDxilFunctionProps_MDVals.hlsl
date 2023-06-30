// RUN: %dxc -fcgl -T lib_6_8 %s | FileCheck %s
//
// Verify that the MDVals data structure inside EmitDxilFunctionProps in 
// DxilMetadataHelper.cpp doesn't get any out-of-bounds assignments.

// CHECK: !36

struct loadStressRecord
{
    uint3 grid : SV_DispatchGrid;
};

void loadStressWorker(
    NodeOutput<loadStressRecord> outputNode)
{
    GroupNodeOutputRecords<loadStressRecord> outRec = outputNode.GetGroupNodeOutputRecords(13);    
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
