// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// RUN: %dxc -T lib_6_8 %s | %D3DReflect %s | FileCheck -check-prefix=REFL %s

// The purpose of this test is to verify the behavior of the compiler on node
// shaders with no input records

// CHECK: = !{void ()* @cs_and_node, !"cs_and_node", null, null, [[ExtAttrs:![0-9]+]]}
// CHECK: [[ExtAttrs]] =
// CHECK-SAME: i32 22, [[NodeMaxDispatchGrid:![0-9]+]],
// CHECK: [[NodeMaxDispatchGrid]] = !{i32 3, i32 1, i32 1}

// REFL:   Inputs: <RecordArrayRef<IONode>[0]> = {}
// REFL: }

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void cs_and_node()
{
}
