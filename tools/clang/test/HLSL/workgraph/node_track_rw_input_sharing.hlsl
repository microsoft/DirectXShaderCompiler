// RUN: %clang_cc1 -HV 2021 -verify %s

struct RECORD
{
  uint a;
  bool b;
  uint3 grid : SV_DispatchGrid;
};

struct RECORD2
{
  uint a;
  bool b;
};

struct [NodeTrackRWInputSharing] TRACKED_RECORD
{
  uint a;
};

//==============================================================================
// Check Get[GroupShared]NodeOutput[Array]() intrinsics don't match with invalid
// parameter types.

[Shader("node")]
[NumThreads(8,1,1)]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(8,1,1)]
// expected-error@+1{{NodeTrackRWInputSharing attribute cannot be applied to Input Records that are not RWDispatchNodeInputRecord}}
void node4_01(DispatchNodeInputRecord<TRACKED_RECORD> input) {
  input.FinishedCrossGroupSharing(); // no error 
}
