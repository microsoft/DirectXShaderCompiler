// RUN: %dxc -T lib_6_8 %s 
// ==================================================================
// Check that validation errors are generated when both compute and
// node are specified and node input or outputs are also present.
// The validation test will add compute to each of the following
// shaders in turn, and check for the expected error message.
// We also check that only Broadcasting nodes may be used with
// compute.
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
