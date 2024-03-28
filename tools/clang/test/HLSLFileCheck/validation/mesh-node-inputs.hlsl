// Source file for altered mesh-node-inputs.ll
// Not intended for indpendent testing

// Run line required in this location, so we'll verify compilation succeeds.
// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// CHECK: define void @node_RWDispatchNodeInputRecord()
// CHECK: define void @node_GroupNodeInputRecords()
// CHECK: define void @node_RWGroupNodeInputRecords()
// CHECK: define void @node_ThreadNodeInputRecord()
// CHECK: define void @node_RWThreadNodeInputRecord()

RWBuffer<uint> buf0;

struct RECORD {
  uint ival;
};

[Shader("node")]
[NumThreads(1024,1,1)]
[NodeDispatchGrid(64,1,1)]
[NodeLaunch("broadcasting")]
void node_RWDispatchNodeInputRecord(RWDispatchNodeInputRecord<RECORD> input) {
  buf0[0] = input.Get().ival;
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
void node_GroupNodeInputRecords(GroupNodeInputRecords<RECORD> input) {
  buf0[0] = input.Get().ival;
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1024,1,1)]
void node_RWGroupNodeInputRecords(RWGroupNodeInputRecords<RECORD> input) {
  buf0[0] = input.Get().ival;
}

[Shader("node")]
[NodeLaunch("thread")]
void node_ThreadNodeInputRecord(ThreadNodeInputRecord<RECORD> input) {
  buf0[0] = input.Get().ival;
}

[Shader("node")]
[NodeLaunch("thread")]
void node_RWThreadNodeInputRecord(RWThreadNodeInputRecord<RECORD> input) {
  buf0[0] = input.Get().ival;
}
