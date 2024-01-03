// RUN: %dxc -T lib_6_8 `%s | FileCheck %s
// ==================================================================
// Broadcasting launch node with class input record
// ==================================================================

class ClassInputRecord
{
  uint a;
};

// CHECK-NOT: error
// CHECK: define void @node01()
// CHECK-NOT: error

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(2,3,2)]
[NumThreads(1024,1,1)]
void node01(DispatchNodeInputRecord<ClassInputRecord> input)
{ }
