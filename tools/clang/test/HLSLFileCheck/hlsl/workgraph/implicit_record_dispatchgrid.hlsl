// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// RUN: %dxc -T lib_6_8 %s | %D3DReflect %s | FileCheck -check-prefix=REFL %s

// The purpose of this test is to verify the behavior of the compiler on node
// shaders with no input records

// CHECK: = !{void ()* @cs_and_node, !"cs_and_node", null, null, [[ExtAttrs:![0-9]+]]}
// CHECK: [[ExtAttrs]] =
// CHECK-SAME: i32 20, [[InputNodes:![0-9]+]],
// CHECK: [[InputNodes]] = !{[[InputNode:![0-9]+]]}
// CHECK: [[InputNode]] = !{i32 1, i32 9}

// REFL: Inputs: <13:RecordArrayRef<IONode>[1]>  = {
// REFL:   [0]: <0:IONode> = {
// REFL:     IOFlagsAndKind: 9
// REFL:     Attribs: <RecordArrayRef<NodeShaderIOAttrib>[0]> = {}
// REFL:   }
// REFL: }

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void cs_and_node()
{
}
