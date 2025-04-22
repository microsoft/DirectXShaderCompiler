// RUN: %dxc -spirv -Od -T lib_6_8 external %s | FileCheck %s

// Renamed node, unnamed index defaults to 0

struct RECORD {
  uint i;
};

[Shader("node")]
[NodeLaunch("thread")]
[NodeID("new_node_name")]
[NodeIsProgramEntry]
void node017_renamed_node([NodeID("output_node_name", 2)] NodeOutput<RECORD> r)
{
  r.GetThreadNodeOutputRecords(1);
}

// CHECK: OpEntryPoint GLCompute %{{[^ ]*}} "node017_renamed_node"
// CHECK-DAG: OpDecorateId [[TYPE:%[^ ]*]] PayloadNodeNameAMDX [[STR:%[0-9A-Za-z_]*]]
// CHECK-DAG: OpDecorateId [[TYPE]] PayloadNodeBaseIndexAMDX [[U2:%[0-9A-Za-z_]*]]
// CHECK: [[UINT:%[^ ]*]] = OpTypeInt 32 0
// CHECK-DAG: [[STR]] = OpConstantStringAMDX "output_node_name"
// CHECK-DAG: [[U2]] = OpConstant [[UINT]] 2
