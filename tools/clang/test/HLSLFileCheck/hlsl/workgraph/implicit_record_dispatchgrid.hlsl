// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// RUN: %dxc -T lib_6_8 %s | %D3DReflect %s | FileCheck -check-prefix=REFL %s

// CHECK: = !{void ()* @cs_and_node, !"cs_and_node", null, null, [[ExtAttrs:![0-9]+]]}
// CHECK: [[ExtAttrs]] =
// CHECK-SAME: i32 20, [[InputNodes:![0-9]+]],
// CHECK: [[InputNodes]] = !{[[InputNode:![0-9]+]]}
// CHECK: [[InputNode]] = !{i32 1, i32 97, i32 2, [[RecordInfo:![0-9]+]]}
// CHECK: [[RecordInfo]] = !{i32 0, i32 12, i32 1, [[SVDispatchGrid:![0-9]+]]}
// CHECK: [[SVDispatchGrid]] = !{i32 0, i32 5, i32 3}

// REFL: Inputs: <15:RecordArrayRef<IONode>[1]>  = {
// REFL:   [0]: <0:IONode> = {
// REFL:     IOFlagsAndKind: 97
// REFL:     Attribs: <12:RecordArrayRef<NodeShaderIOAttrib>[2]>  = {
// REFL:       [0]: <0:NodeShaderIOAttrib> = {
// REFL:         AttribKind: RecordSizeInBytes
// REFL:         RecordSizeInBytes: 12
// REFL:       }
// REFL:       [1]: <1:NodeShaderIOAttrib> = {
// REFL:         AttribKind: RecordDispatchGrid
// REFL:         RecordDispatchGrid: <RecordDispatchGrid>
// REFL:           ByteOffset: 0
// REFL:           ComponentNumAndType: 23
// REFL:       }
// REFL:     }
// REFL:   }
// REFL: }

[Shader("node")]
[NodeMaxDispatchGrid(3, 1, 1)]
[NumThreads(16, 1, 1)]
void cs_and_node()
{
}
