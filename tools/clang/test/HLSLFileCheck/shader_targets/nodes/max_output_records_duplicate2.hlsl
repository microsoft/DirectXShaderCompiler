// RUN: %dxc -T lib_6_8  %s | FileCheck %s

// Duplicate MaxRecords info with mismatching limits

// CHECK: 27:68: error: parameter 'Output2' may not have both 'maxrecordssharedwith' and 'MaxRecords'
// CHECK: 28:68: error: parameter 'Output3' may not have both 'maxrecordssharedwith' and 'MaxRecords'
// CHECK: 29:68: error: parameter 'Output4' may not have both 'maxrecordssharedwith' and 'MaxRecords'
// CHECK: 30:68: error: parameter 'Output5' may not have both 'maxrecordssharedwith' and 'MaxRecords'

struct rec0
{
    int i0;
    float f0;
};

struct rec1
{
    float f1;
    int i1;
};

[Shader("node")]
[NodeLaunch("Thread")]
void DuplicateMax1(
  RWThreadNodeInputRecord<rec0> InputyMcInputFace,
  [MaxRecords(1)] NodeOutput<rec1> Output1,
  [MaxRecordsSharedWith(Output1)] [MaxRecords(2)] NodeOutput<rec1> Output2,
  [MaxRecords(3)] [MaxRecordsSharedWith(Output1)] NodeOutput<rec1> Output3,
  [MaxRecordsSharedWith(Output6)] [MaxRecords(4)] NodeOutput<rec1> Output4,
  [MaxRecords(5)] [MaxRecordsSharedWith(Output6)] NodeOutput<rec1> Output5,
  [MaxRecords(6)] NodeOutput<rec1> Output6)
{
}
