// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// If no input is specified then the NodeInputs metadata should not
// be present.
// ==================================================================

[Shader("node")]
[NodeLaunch("mesh")]
[NumThreads(1,1,1)]
[NodeDispatchGrid(4,1,1)]
void noinput_mesh() { }



// Metadata for noinput_mesh
// ------------------------------------------------------------------
// CHECK: = !{void ()* @noinput_mesh, !"noinput_mesh", null, null, [[ATTRS_MESH:![0-9]+]]}

// Metadata for node attributes
// Arg #1: ShaderKind Tag (8)
// Arg #2: Node (15)
// Arg #3: NodeLaunch Tag (13)
// Arg #4: mesh (1)
// ...
// ------------------------------------------------------------------
// CHECK: [[ATTRS_MESH]] = !{
// CHECK-SAME: i32 8, i32 15, i32 13, i32 4,
// CHECK-NOT: i32 20, i32 {{![-=9]+}}
// CHECK-SAME: }

