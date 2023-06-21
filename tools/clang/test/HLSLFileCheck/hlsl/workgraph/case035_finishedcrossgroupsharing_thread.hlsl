// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE035 (fail)
// FinishedCrossGroupSharing() is called in a thread launch node
// ==================================================================

struct [NodeTrackRWInputSharing] INPUT_RECORD {
  uint value;
};

[Shader("node")]
[NodeLaunch("Thread")]
[NodeIsProgramEntry]
void node035_finishedcrossgroupsharing_thread(RWThreadNodeInputRecord<INPUT_RECORD> input)
{
  bool foo = input.FinishedCrossGroupSharing();
}

// CHECK: 16:20: error: no member named 'FinishedCrossGroupSharing' in 'RWThreadNodeInputRecord<INPUT_RECORD>'
// CHECK-NEXT:  bool foo = input.FinishedCrossGroupSharing();
// CHECK-NEXT:             ~~~~~ ^

