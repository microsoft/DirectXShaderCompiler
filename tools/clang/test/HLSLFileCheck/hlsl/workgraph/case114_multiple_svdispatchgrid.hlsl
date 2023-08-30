// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// Input record with multiple fields with SV_DispatchGrid annotation
// ==================================================================

struct INPUT_RECORD
{
  uint DispatchGrid1 : SV_DispatchGrid;
  uint2 a;
  uint3 DispatchGrid2 : SV_DispatchGrid;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(256,1,1)]
[NumThreads(1024,1,1)]
void node114_multiple_svdispatchgrid(DispatchNodeInputRecord<INPUT_RECORD> input)
{
}

// CHECK: :10:25: error: a field with SV_DispatchGrid has already been specified
// CHECK-NEXT: uint3 DispatchGrid2 : SV_DispatchGrid;
// CHECK-NEXT:                       ^
// CHECK-NEXT: :8:24: note: previously defined here
// CHECK-NEXT: uint DispatchGrid1 : SV_DispatchGrid;
// CHECK-NEXT:                      ^

