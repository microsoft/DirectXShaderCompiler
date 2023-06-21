// RUN: %dxc -T lib_6_8 -DTYPE=DispatchNodeInputRecord -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=RWDispatchNodeInputRecord -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=GroupNodeInputRecords -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=RWGroupNodeInputRecords -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=ThreadNodeInputRecord -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=RWThreadNodeInputRecord -DARGS=<EMPTY> %s | FileCheck %s
// RUN: %dxc -T lib_6_8 -DTYPE=NodeOutput -DARGS=<EMPTY> %s | FileCheck %s
// ==================================================================
// zero-sized-node-record (expected error)
// An error diagnostic is generated for a zero sized record used in
// a node input/output declaration.
// N.B. We use multiple run lines as only the first CodeGen error is
// reported
// ==================================================================

struct EMPTY {
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NumThreads(1,1,1)]
void node0(TYPE ARGS a) { }

// CHECK: 22:12: error: record used in {{DispatchNodeInputRecord|RWDispatchNodeInputRecord|GroupNodeInputRecords|RWGroupNodeInputRecords|ThreadNodeInputRecord|RWThreadNodeInputRecord|NodeOutput}} may not have zero size
// CHECK: 16:8: note: zero sized record defined here
