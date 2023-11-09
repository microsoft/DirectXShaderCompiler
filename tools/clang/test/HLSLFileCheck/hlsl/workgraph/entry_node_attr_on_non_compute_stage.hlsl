// RUN: %dxc -T cs_6_6 -E main %s | FileCheck %s

struct [NodeTrackRWInputSharing] loadStressRecord {
    uint x : SV_DispatchGrid;
};

void loadStressWorker(NodeOutput<loadStressRecord> x) {
    return;
}

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void main(
    // CHECK: error: NodeTrackRWInputSharing attribute cannot be applied to Input Records that are not RWDispatchNodeInputRecord
    DispatchNodeInputRecord<loadStressRecord> input,
    [MaxRecords(16)] NodeOutput<loadStressRecord> loadStressChild
)
{
    loadStressWorker(loadStressChild);
}
