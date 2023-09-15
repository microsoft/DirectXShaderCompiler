// RUN: %clang_cc1 -verify %s
// ==================================================================
// OutputComplete() is called with unsupported node i/o types
// ==================================================================

struct RECORD {
  int i;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(1,1,1)]
void node127_a(DispatchNodeInputRecord<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'DispatchNodeInputRecord<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(1,1,1)]
void node127_c(RWDispatchNodeInputRecord<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'RWDispatchNodeInputRecord<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Coalescing")]
[NumThreads(1,1,1)]
void node127_a(GroupNodeInputRecords<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'GroupNodeInputRecords<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Coalescing")]
[NumThreads(1,1,1)]
void node127_c(RWGroupNodeInputRecords<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'RWGroupNodeInputRecords<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Thread")]
[NumThreads(1,1,1)]
void node127_a(ThreadNodeInputRecord<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'ThreadNodeInputRecord<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Thread")]
[NumThreads(1,1,1)]
void node127_c(RWThreadNodeInputRecord<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'RWThreadNodeInputRecord<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(1,1,1)]
void node127_f(NodeOutput<RECORD> nodeOutput)
{
  nodeOutput.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'NodeOutput<RECORD>'}}
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NumThreads(1,1,1)]
[NodeMaxDispatchGrid(1,1,1)]
void node127_g(EmptyNodeOutput emptyNodeOutput)
{
  emptyNodeOutput.OutputComplete(); // expected-error {{no member named 'OutputComplete' in 'EmptyNodeOutput'}}
}
