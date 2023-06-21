// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE034 (fail)
// FinishedCrossGroupSharing() is called in a coalescing launch node
// ==================================================================

struct [NodeTrackRWInputSharing] INPUT_RECORD {
  uint value;
};

[Shader("node")]
[NodeLaunch("Coalescing")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node034_finishedcrossgroupsharing_coalescing(RWGroupNodeInputRecords<INPUT_RECORD> input)
{
  bool foo = input.FinishedCrossGroupSharing();
}

// CHECK: 17:20: error: no member named 'FinishedCrossGroupSharing' in 'RWGroupNodeInputRecords<INPUT_RECORD>'
// CHECK-NEXT:   bool foo = input.FinishedCrossGroupSharing();
// CHECK-NEXT:             ~~~~~ ^
