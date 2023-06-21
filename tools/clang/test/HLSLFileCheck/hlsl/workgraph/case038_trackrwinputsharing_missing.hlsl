// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE038 (fail)
// FinishedCrossGroupSharing() is called without NodeTrackRWInputSharing
// ==================================================================

struct INPUT_RECORD
{
  uint value;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(256,1,1)]
[NumThreads(1,1,1)]
[NodeIsProgramEntry]
void node038_trackrwinputsharing_missing(RWDispatchNodeInputRecord<INPUT_RECORD> input)
{
  bool bar = input.FinishedCrossGroupSharing();
}

// CHECK: :19:14: error: Use of FinishedCrossGroupSharing() requires NodeTrackRWInputSharing attribute to be specified on the record struct type
// CHECK-NEXT:  bool bar = input.FinishedCrossGroupSharing();
// CHECK-NEXT:             ^
