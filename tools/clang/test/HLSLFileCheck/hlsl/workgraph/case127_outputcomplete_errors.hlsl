// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE127 (fail)
// OutputComplete() is called with unsupported node i/o types
// ==================================================================

struct RECORD {
  int i;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
void node127_a(DispatchNodeInputRecord<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
void node127_c(RWDispatchNodeInputRecord<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Coalescing")]
void node127_a(GroupNodeInputRecords<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Coalescing")]
void node127_c(RWGroupNodeInputRecords<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Thread")]
void node127_a(ThreadNodeInputRecord<RECORD> nodeInputRecord)
{
  nodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Thread")]
void node127_c(RWThreadNodeInputRecord<RECORD> rwNodeInputRecord)
{
  rwNodeInputRecord.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
void node127_e([MaxRecords(5)] EmptyNodeInput emptyNodeInput)
{
  emptyNodeInput.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
void node127_f(NodeOutput<RECORD> nodeOutput)
{
  nodeOutput.OutputComplete();
}

[Shader("node")]
[NodeLaunch("Broadcasting")]
void node127_g(EmptyNodeOutput emptyNodeOutput)
{
  emptyNodeOutput.OutputComplete();
}

// TODO: NODE OUTPUT ARRAY - not implemented yet
// [Shader("node")]
// [NodeLaunch("Broadcasting")]
// void node127_h(NodeOutputArray<RECORD> nodeOutput_array[8])
// {
//   nodeOutput_array[3].OutputComplete();
// }

// CHECK: 15:19: error: no member named 'OutputComplete' in 'DispatchNodeInputRecord<RECORD>'
// CHECK:   nodeInputRecord.OutputComplete();
// CHECK:  ~~~~~~~~~~~~~~~ ^

// CHECK: 22:21: error: no member named 'OutputComplete' in 'RWDispatchNodeInputRecord<RECORD>'
// CHECK:   rwNodeInputRecord.OutputComplete();
// CHECK:   ~~~~~~~~~~~~~~~~~ ^

// CHECK: 29:19: error: no member named 'OutputComplete' in 'GroupNodeInputRecords<RECORD>'
// CHECK:   nodeInputRecord.OutputComplete();
// CHECK:   ~~~~~~~~~~~~~~~ ^

// CHECK: 36:21: error: no member named 'OutputComplete' in 'RWGroupNodeInputRecords<RECORD>'
// CHECK:   rwNodeInputRecord.OutputComplete();
// CHECK:   ~~~~~~~~~~~~~~~~~ ^

// CHECK: 43:19: error: no member named 'OutputComplete' in 'ThreadNodeInputRecord<RECORD>'
// CHECK:   nodeInputRecord.OutputComplete();
// CHECK:   ~~~~~~~~~~~~~~~ ^

// CHECK: 50:21: error: no member named 'OutputComplete' in 'RWThreadNodeInputRecord<RECORD>'
//   rwNodeInputRecord.OutputComplete();
// CHECK:   ~~~~~~~~~~~~~~~~~ ^

// CHECK: 57:18: error: no member named 'OutputComplete' in 'EmptyNodeInput'
//   emptyNodeInput.OutputComplete();
//   ~~~~~~~~~~~~~~ ^

// CHECK: 64:14: error: no member named 'OutputComplete' in 'NodeOutput<RECORD>'
//   emptyNodeInput.OutputComplete();
//   ~~~~~~~~~~~~~~ ^

// CHECK: 71:19: error: no member named 'OutputComplete' in 'EmptyNodeOutput'
//   emptyNodeInput.OutputComplete();
//   ~~~~~~~~~~~~~~ ^

// TODO: NODE OUTPUT ARRAY - not implemented yet
// CHECK-not: 78:23: error: no member named 'OutputComplete' in 'NodeOutputArray<RECORD>'
//   emptyNodeInput.OutputComplete();
//   ~~~~~~~~~~~~~~ ^
