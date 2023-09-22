// RUN: %clang_cc1 -fsyntax-only -verify %s
// NodeOutputArray and EmptyNodeOutputArray are provided in place of
// NodeOutput and EmptyNodeOutput native array parameters.
// Check error diagnostics are produced for such native array parameters.

struct Record {
  uint a;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NodeIsProgramEntry]
[NumThreads(32, 1, 1)]
void node01(NodeOutput<Record> output[9]) // expected-error {{arrays of 'NodeOutput' are not supported as node parameters - use 'NodeOutputArray' instead}}
{ }

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(32, 1, 1)]
void node02(EmptyNodeOutput output[4]) // expected-error {{arrays of 'EmptyNodeOutput' are not supported as node parameters - use 'EmptyNodeOutputArray' instead}}
{ }
