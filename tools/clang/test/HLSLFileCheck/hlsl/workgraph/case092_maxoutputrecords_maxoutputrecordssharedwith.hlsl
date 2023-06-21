// RUN: %dxc -T lib_6_8 %s | FileCheck %s
// ==================================================================
// CASE092 (fail)
// MaxRecords and MaxRecordsSharedWith are both declared
// ==================================================================

struct INPUT_RECORD
{
  uint value;
};

struct OUTPUT_RECORD
{
  uint num;
};

[Shader("node")]
[NodeLaunch("Broadcasting")]
[NodeDispatchGrid(256,1,1)]
[NumThreads(1,1,1)]
[NodeIsProgramEntry]
void node092_maxoutputrecords_maxoutputrecordssharedwith(DispatchNodeInputRecord<INPUT_RECORD> input,
                                                         [MaxRecords(5)] NodeOutput<OUTPUT_RECORD> firstOut,
                                                         [MaxRecords(5)][MaxRecordsSharedWith(firstOut)] NodeOutput<OUTPUT_RECORD> secondOut)
{
}

// CHECK: 24:132: error: Only one of MaxRecords or MaxRecordsSharedWith may be specified to the same parameter
