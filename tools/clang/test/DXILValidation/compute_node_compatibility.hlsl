// ==================================================================
// Errors are expected for shaders with both "node" and "compute"
// specified when:
// - a broadcasting node has an input record and/or output records
// - the launch type is not broadcasting
// This test operates by changing the [Shader(node)] metadata entry
// to [Shader(compute)] as if both had been specified in the HLSL,
// for each shader in turn.
// It also changes the coalescing to thread in the final test case.
// ==================================================================

struct RECORD {
  uint a;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node01(DispatchNodeInputRecord<RECORD> input) { }

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(1,1,1)]
void node02(RWDispatchNodeInputRecord<RECORD> input) { }

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(3, 1, 1)]
[NumThreads(1,1,1)]
void node03(NodeOutput<RECORD> output) { }

[Shader("node")]
[NodeLaunch("Coalescing")]
[NumThreads(1,1,1)]
void node04() { }
