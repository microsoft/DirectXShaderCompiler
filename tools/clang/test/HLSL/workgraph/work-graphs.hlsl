// RUN: %clang_cc1 -HV 2021 -verify %s

struct RECORD
{
  uint a;
  bool b;
};

struct RECORD2
{
  uint a;
  bool b;
};

struct BAD_RECORD
{
  uint a;
  SamplerState s;
};

struct BAD_RECORD2
{
  BAD_RECORD b;
};

struct [NodeTrackRWInputSharing] TRACKED_RECORD
{
  uint a;
};

//==============================================================================
// Check diagnostics for the various node input/output types

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_01(DispatchNodeInputRecord<int> input) /* expected-error {{'int' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(8,1,1)]
void node1_02(GroupNodeInputRecords<float> input) /* expected-error {{'float' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(2,1,1)]
void node1_03(GroupNodeInputRecords<SamplerState> input) /* expected-error {{'SamplerState' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_04(RWDispatchNodeInputRecord<RECORD[2]> input) /* expected-error {{'RECORD [2]' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(8,1,1)]
void node1_05(RWGroupNodeInputRecords<float> input) /* expected-error {{'float' cannot be used as a type parameter where a struct/class is required}} */
{ }

typedef matrix<float,2,2> f2x2;

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_06(RWDispatchNodeInputRecord<f2x2,4> input) /* expected-error {{too many template arguments for class template 'RWDispatchNodeInputRecord'}}  */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_07(NodeOutput<bool> output) /* expected-error {{'bool' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_08(NodeOutput<RECORD[3]> output) /* expected-error {{'RECORD [3]' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_09(NodeOutput<float4> output) /* expected-error {{'float4' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_10(DispatchNodeInputRecord<float3> input) /* expected-error {{'float3' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_11(DispatchNodeInputRecord<BAD_RECORD> input) /* expected-error {{'BAD_RECORD' cannot be used as a type parameter where a struct/class is required}} */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_12(RWDispatchNodeInputRecord<BAD_RECORD2> input) /* expected-error {{'BAD_RECORD2' cannot be used as a type parameter where a struct/class is required}} */
{ }


[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node1_16()
{
  GroupNodeOutputRecords<int> outrec1; /* expected-error {{'int' cannot be used as a type parameter where a struct/class is required}} */

  ThreadNodeOutputRecords<bool> outrec2; /* expected-error {{'bool' cannot be used as a type parameter where a struct/class is required}} */

  GroupNodeOutputRecords<float4> outrec3; /* expected-error {{'float4' cannot be used as a type parameter where a struct/class is required}} */

  ThreadNodeOutputRecords<RECORD[4]> outrec4; /* expected-error {{'RECORD [4]' cannot be used as a type parameter where a struct/class is required}} */
}


//==============================================================================
// Check Get[GroupShared]NodeOutput[Array]() intrinsics don't match with invalid
// parameter types.

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node2_01([MaxRecords(5)] EmptyNodeOutput output)
{
  // GetGroupNodeOutputRecords() is called on an EmptyNodeOutput
  output.GetGroupNodeOutputRecords(1);   /* expected-error {{no member named 'GetGroupNodeOutputRecords' in 'EmptyNodeOutput'}} */
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node2_02([MaxRecords(5)] EmptyNodeOutput output)
{
  // GetThreadNodeOutputRecords() is called on an EmptyNodeOutput
  output.GetThreadNodeOutputRecords(3);   /* expected-error {{no member named 'GetThreadNodeOutputRecords' in 'EmptyNodeOutput'}} */
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node2_05(DispatchNodeInputRecord<RECORD> input)
{
  // GetGroupNodeOutputRecords() is called on a DispatchNodeInputRecord<>
  input.GetGroupNodeOutputRecords(1);   /* expected-error {{no member named 'GetGroupNodeOutputRecords' in 'DispatchNodeInputRecord<RECORD>'}} */
}

template<typename T>
struct FakeNodeOutput {
  int h;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node2_06(FakeNodeOutput<RECORD> output)
{
  // GetGroupNodeOutputRecords() is called on a type that is like NodeOutput<> to check INTRIN_COMPTYPE_FROM_NODEOUTPUT isn't fooled.
  GroupNodeOutputRecords<RECORD> outrec = output.GetGroupNodeOutputRecords(1); /* expected-error {{no member named 'GetGroupNodeOutputRecords' in 'FakeNodeOutput<RECORD>'}} */
}

//==============================================================================
// Check invalid initialization of *NodeOutputRecords

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node3_01(NodeOutput<RECORD> output)
{
  // Initializing a GroupNodeOutputRecords<RECORD2> from NodeOutput<RECORD>::GetNodeOutput()
  GroupNodeOutputRecords<RECORD2> outrec = output.GetGroupNodeOutputRecords(5); /* expected-error {{cannot initialize a variable of type 'GroupNodeOutputRecords<RECORD2>' with an rvalue of type 'GroupNodeOutputRecords<RECORD>'}} */
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node3_02(NodeOutput<RECORD> output)
{
  // Initializing a ThreadNodeOutputRecords<RECORD2> from NodeOutput<RECORD>::ThreadNodeOutputRecords()
  ThreadNodeOutputRecords<RECORD2> outrec = output.GetThreadNodeOutputRecords(5); /* expected-error {{cannot initialize a variable of type 'ThreadNodeOutputRecords<RECORD2>' with an rvalue of type 'ThreadNodeOutputRecords<RECORD>'}} */
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node3_03(NodeOutput<RECORD> output)
{
  // Initializing a ThreadNodeOutputRecords<RECORD> from GetGroupNodeOutputRecords() 
  ThreadNodeOutputRecords<RECORD> outrec = output.GetGroupNodeOutputRecords(1); /* expected-error {{cannot initialize a variable of type 'ThreadNodeOutputRecords<RECORD>' with an rvalue of type 'GroupNodeOutputRecords<RECORD>'}} */
}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node3_04(NodeOutput<RECORD> output)
{
  // Initializing a GroupNodeOutputRecords<RECORD> from GetThreadNodeOutputRecords() 
  GroupNodeOutputRecords<RECORD> outrec = output.GetThreadNodeOutputRecords(1); /* expected-error {{cannot initialize a variable of type 'GroupNodeOutputRecords<RECORD>' with an rvalue of type 'ThreadNodeOutputRecords<RECORD>'}} */
}

//==============================================================================
// Check invalid template arguments

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node4_01(DispatchNodeInputRecord<RECORD, 20> input) /* expected-error {{too many template arguments for class template 'DispatchNodeInputRecord'}}  */
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node4_02(ThreadNodeInputRecord input) /* expected-error {{use of class template 'ThreadNodeInputRecord' requires template arguments}}  */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node4_03(DispatchNodeInputRecord<Texture2D> input) /* expected-error {{Texture2D cannot be used as a type parameter where a struct/class is required}}  */
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node4_04(DispatchNodeInputRecord<RaytracingAccelerationStructure> input) /* expected-error {{'RaytracingAccelerationStructure' cannot be used as a type parameter where a struct/class is required}}  */
{ }


//==============================================================================
// Check FinishedCrossGroupSharing only available for RWDispatchNodeInputRecord
// in Broadcasting launch nodes

[Shader("node")]
[NumThreads(8,1,1)]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(8,1,1)]
void node4_01(RWDispatchNodeInputRecord<TRACKED_RECORD> input) {
  input.FinishedCrossGroupSharing(); // no error 
}


[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeDispatchGrid(8,1,1)]
void node4_02(DispatchNodeInputRecord<RECORD> input) {
  input.FinishedCrossGroupSharing(); // expected-error {{no member named 'FinishedCrossGroupSharing' in 'DispatchNodeInputRecord<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node4_03(GroupNodeInputRecords<RECORD> input)
{
  bool foo = input.FinishedCrossGroupSharing(); // expected-error {{no member named 'FinishedCrossGroupSharing' in 'GroupNodeInputRecords<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
[NodeIsProgramEntry]
void node4_04(RWGroupNodeInputRecords<RECORD> input)
{
  bool foo = input.FinishedCrossGroupSharing(); // expected-error {{no member named 'FinishedCrossGroupSharing' in 'RWGroupNodeInputRecords<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("thread")]
[NodeIsProgramEntry]
void node4_05(ThreadNodeInputRecord<RECORD> input)
{
  input.FinishedCrossGroupSharing(); // expected-error {{no member named 'FinishedCrossGroupSharing' in 'ThreadNodeInputRecord<RECORD>'}}
}


[Shader("node")]
[NodeLaunch("thread")]
[NodeIsProgramEntry]
void node4_06(RWThreadNodeInputRecord<RECORD> input)
{
  input.FinishedCrossGroupSharing(); // expected-error {{no member named 'FinishedCrossGroupSharing' in 'RWThreadNodeInputRecord<RECORD>'}}
}
