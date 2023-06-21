// RUN: %dxc -T lib_6_8  %s | FileCheck %s

// Test maxoutputrecordssharedwith with invalid references

// CHECK: 26:52: error: MaxRecordsSharedWith must reference a valid ouput parameter name.
// CHECK: 28:62: error: MaxRecordsSharedWith must reference a valid ouput parameter name.
// CHECK: 30:52: error: MaxRecordsSharedWith must not reference the same parameter it is applied to.

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
void InvalidRef(
  RWThreadNodeInputRecord<rec0> InputyMcInputFace,
  // MaxRecordsSharedWith referencing non-existant parameter
  [MaxRecordsSharedWith(Output7)] NodeOutput<rec1> Output1,
  // MaxRecordsSharedWith referencing an input parameter
  [MaxRecordsSharedWith(InputyMcInputFace)] NodeOutput<rec1> Output2,
  // MaxRecordsSharedWith referencing its own parameter
  [MaxRecordsSharedWith(Output3)] NodeOutput<rec1> Output3,
  [MaxRecords(5)] NodeOutput<rec1> Output4)
{
}
