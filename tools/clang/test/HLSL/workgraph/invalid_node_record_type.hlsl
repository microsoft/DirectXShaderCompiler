// RUN: %clang_cc1 -HV 2021 -verify %s

struct RECORD
{
  uint a;
  bool b;
  float4 c;
  matrix<float,3,3> d;
};

struct BAD_RECORD
{
  uint a;
  RECORD rec;
  SamplerState s; // expected-note+ {{'SamplerState' field declared here}}
};

struct BAD_RECORD2
{
  BAD_RECORD bad;
};

typedef matrix<float,2,2> f2x2;

typedef SamplerState MySamplerState;

typedef BAD_RECORD2 MyBadRecord;

//==============================================================================
// Check diagnostics for the various node input/output types

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node01(DispatchNodeInputRecord<int> input) // expected-error {{'int' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(8,1,1)]
void node02(GroupNodeInputRecords<float4> input) // expected-error {{'float4' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node03(ThreadNodeInputRecord<int2x2> input) // expected-error {{'int2x2' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node04(RWDispatchNodeInputRecord<int[8]> input) // expected-error {{'int [8]' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(2,1,1)]
void node05(RWGroupNodeInputRecords<SamplerState> input) // expected-error {{'SamplerState' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node06(RWThreadNodeInputRecord<matrix<float,4,4> > input) // expected-error {{'matrix<float, 4, 4>' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node07(RWThreadNodeInputRecord<f2x2> input) // expected-error {{'f2x2' (aka 'matrix<float, 2, 2>') is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node08(ThreadNodeInputRecord<BAD_RECORD> input) // expected-error {{object 'SamplerState' may not appear in a node record}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node09(ThreadNodeInputRecord<BAD_RECORD[4]> input) // expected-error {{'BAD_RECORD [4]' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node10(RWThreadNodeInputRecord<BAD_RECORD2> input) // expected-error {{object 'SamplerState' may not appear in a node record}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node11(NodeOutput<BAD_RECORD> input) // expected-error {{object 'SamplerState' may not appear in a node record}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node12(NodeOutputArray<MyBadRecord> output) // expected-error {{object 'SamplerState' may not appear in a node record}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node13(NodeOutputArray<DispatchNodeInputRecord<RECORD> > output) // expected-error {{'DispatchNodeInputRecord<RECORD>' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("thread")]
void node14(ThreadNodeInputRecord<matrix<float,4,4> > input) // expected-error {{'matrix<float, 4, 4>' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node15(NodeOutputArray<RaytracingAccelerationStructure> output) // expected-error {{'RaytracingAccelerationStructure' is not valid as a node record type - struct/class required}}
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void node16()
{
  GroupNodeOutputRecords<int> outrec1; // expected-error {{'int' is not valid as a node record type - struct/class required}}

  ThreadNodeOutputRecords<f2x2> outrec2; // expected-error {{'f2x2' (aka 'matrix<float, 2, 2>') is not valid as a node record type - struct/class required}}

  GroupNodeOutputRecords<MyBadRecord> outrec3; // expected-error {{object 'SamplerState' may not appear in a node record}}

  ThreadNodeOutputRecords<SamplerState> outrec4; // expected-error {{'SamplerState' is not valid as a node record type - struct/class required}}
}


// TODO: templated structs should be handled as any other structs are - allowed where valid, and diagnostics generated
// where invalid.
// Unfortunately templated structs are not currently handled correctly by either the DiagnodeNodeRecordStructArgument()
// function (always producing a zero sized record diagnostic), or if that is bypassed they hit asserts in CodeGen.
// NOTE: for now, an error diagnostic is produced for the use of a templated type in a node record.
template<typename T> struct MyTemplateStruct {
  T MyField;
};

struct MyNestedTemplateStruct {
  MyTemplateStruct<SamplerState> a; // expected-note {{templated field declared here}}
};

// TODO: This should be accepted without error
[Shader("node")]
[NodeLaunch("thread")]
void node17(ThreadNodeInputRecord<MyTemplateStruct<int> > input) // expected-error {{templated types are not currently supported in a node record}}
{ }

// This should generate a "object 'SamplerState' may not appear in a node record" diagnostic
[Shader("node")]
[NodeLaunch("thread")]
void node18(ThreadNodeInputRecord<MyTemplateStruct<SamplerState> > input) // expected-error {{templated types are not currently supported in a node record}}
{ }

// This should generate a "object 'SamplerState' may not appear in a node record" diagnostic
[Shader("node")]
[NodeLaunch("thread")]
void node19(RWThreadNodeInputRecord<MyNestedTemplateStruct> input) // expected-error {{templated types are not currently supported in a node record}}
{ }
