// RUN: %dxc /T ms_6_5 -E main -fcgl %s | FileCheck %s

// Verify that SetMeshOutputCounts is emitted with the noduplicate function
// attribute during AST->IR generation. This prevents passes such as
// JumpThreading from cloning the call when its arguments are computed in a
// scalar branch (see GitHub issue #8104).

// CHECK: call void @"dx.hl.op.nd.void (i32, i32, i32)"(i32 68,
// CHECK: declare void @"dx.hl.op.nd.void (i32, i32, i32)"(i32, i32, i32) [[ATTR:#[0-9]+]]
// CHECK: attributes [[ATTR]] = { noduplicate nounwind }

struct Payload { uint nv; uint np; };

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(in payload Payload pl) {
  SetMeshOutputCounts(pl.nv, pl.np);
}
