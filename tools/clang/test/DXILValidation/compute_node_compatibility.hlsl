// ==================================================================
// Errors are expected for shaders with both "node" and "compute"
// specified when:
// - the launch type is not broadcasting
// - a broadcasting node has an input record and/or output records
// This test operates by changing the [Shader(node)] metadata entry
// to [Shader(compute)] as if both had been specified in the HLSL,
// for each shader in turn.
// ==================================================================

struct RECORD {
  uint a;
};

//[Shader("compute")]
[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("coalescing")]
void node01() { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("coalescing")]
void node02(GroupNodeInputRecords<RECORD> input) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("coalescing")]
void node03(RWGroupNodeInputRecords<RECORD> input) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(128,1,1)]
[NodeLaunch("coalescing")]
void node04(EmptyNodeInput input) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(1,1,1)]
[NodeLaunch("thread")]
void node05() { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(1,1,1)]
[NodeLaunch("thread")]
void node06(ThreadNodeInputRecord<RECORD> input) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(1,1,1)]
[NodeLaunch("thread")]
void node07(RWThreadNodeInputRecord<RECORD> input) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(128,1,1)]
void node08(DispatchNodeInputRecord<RECORD> input) { }

//[Shader("compute")]
[NumThreads(1024,1,1)]
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(128,1,1)]
void node09(RWDispatchNodeInputRecord<RECORD> input) { }

//[Shader("compute")]
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1024,1,1)]
[NodeDispatchGrid(128,1,1)]
void node10(NodeOutput<RECORD> output) { }

//[Shader("compute")]
[Shader("node")]
[NumThreads(1024,1,1)]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(128,1,1)]
void node11(EmptyNodeOutput output) { }
