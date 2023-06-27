// RUN: %dxc -fcgl -T lib_6_8 %s | FileCheck -check-prefix=CHECK_FCGL %s
//

// This test will fail if MDVals in DxilMetadataHelper.cpp gets assigned out of bounds.
// The test is primarily from called_function_arg_nodeoutput.hlsl

// Verify that the correct parameter, the NodeOutputRecord parameter, gets the cast
// CHECK_FCGL: call %"class.NodeOutput<loadStressRecord>"* @"dx.hl.cast..

struct loadStressRecord
{
    uint3 grid : SV_DispatchGrid;
};

void loadStressWorker(
    NodeOutput<loadStressRecord> outputNode)
{
    NodeOutputRecord<loadStressRecord> outRec = GetNodeOutputRecord(outputNode, true);    
    outRec.Get().grid = uint3(39, 61, 71);
}

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void loadStress_16(    
    [MaxOutputRecords(16)] NodeOutput<loadStressRecord> loadStressChild
)
{
    loadStressWorker(loadStressChild);
}
