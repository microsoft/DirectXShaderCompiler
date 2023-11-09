// RUN: %dxc -T ps_6_6 -E main %s | FileCheck %s

struct loadStressRecord {
    uint x : SV_DispatchGrid;
};

void loadStressWorker(NodeOutput<loadStressRecord> x) {
    return;
}

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void main(
    // CHECK: error: entry type 'pixel' from profile 'ps_6_6' conflicts with shader attribute type 'node' on entry function 'main'.
    DispatchNodeInputRecord<loadStressRecord> input,
    [MaxRecords(16)] NodeOutput<loadStressRecord> loadStressChild
)
{
    loadStressWorker(loadStressChild);
}
