// ==================================================================
// Errors are expected for compute shaders when:
// - a broadcasting node has an input record and/or output records
// - the launch type is not broadcasting
// This test operates by changing the [Shader("node")] metadata entry
// to [Shader("compute")] for each shader in turn.
// It also changes the coalescing to thread in the final test case,
// after swapping out the shader kind to compute.
// The shader is compiled with "-T lib_6_8 -HV 2021"
// ==================================================================

struct RECORD {
  uint a;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node01(DispatchNodeInputRecord<RECORD> input) { }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(1,1,1)]
void node02(RWDispatchNodeInputRecord<RECORD> input) { }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(3, 1, 1)]
[NumThreads(1,1,1)]
void node03(NodeOutput<RECORD> output) { }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1,1,1)]
void node04() { }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node05() { }