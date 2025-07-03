// RUN: %dxc -spirv -Vd -Od -T lib_6_8 -fspv-target-env=vulkan1.3 %s | FileCheck %s

// Note: validation disabled until NodePayloadAMDX pointers are allowed
// as function arguments

struct InputPayload {
  uint grid : SV_DispatchGrid;
};

struct OutputPayload {
  uint foo;
};

[[vk::constant_id(0)]]
const uint MaxPayloads = 1;
[[vk::constant_id(1)]]
const uint WorkgroupSizeX = 1;
[[vk::constant_id(2)]]
const uint ShaderIndex = 0;
[[vk::constant_id(3)]]
const uint NumThreadsX = 512;

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(NumThreadsX, 1, 1)]
[NodeDispatchGrid(WorkgroupSizeX, 1, 1)]
 
void main(const uint svGroupIndex : SV_GroupIndex,
          DispatchNodeInputRecord<InputPayload> inputRecord,
          [NodeID("main", ShaderIndex)]
          [MaxRecords(MaxPayloads)]
          NodeOutput<OutputPayload> nodeOutput) {
  ThreadNodeOutputRecords<OutputPayload> outRec = nodeOutput.GetThreadNodeOutputRecords(1);
  outRec.OutputComplete();
}

// CHECK: OpExecutionModeId %{{[_0-9A-Za-z]*}} LocalSizeId [[NUMTHREADSX:%[_0-9A-Za-z]*]] [[U1:%[_0-9A-Za-z]*]] [[U1]]
// CHECK: OpExecutionModeId %{{[_0-9A-Za-z]*}} StaticNumWorkgroupsAMDX [[WGSIZEX:%[_0-9A-Za-z]*]] [[U1]] [[U1]]
// CHECK: OpDecorate [[MAXPAYLOADS:%[_0-9A-Za-z]*]] SpecId 0
// CHECK: OpDecorate [[WGSIZEX]] SpecId 1
// CHECK: OpDecorate [[SHADERINDEX:%[_0-9A-Za-z]*]] SpecId 2
// CHECK: OpDecorate [[NUMTHREADSX]] SpecId 3
// CHECK: OpDecorateId %{{[_0-9A-Za-z]*}} NodeMaxPayloadsAMDX [[U1:%[_0-9A-Za-z]*]]
// CHECK-DAG: OpDecorateId %{{[_0-9A-Za-z]*}} PayloadNodeBaseIndexAMDX [[SHADERINDEX]]
// CHECK-DAG: OpDecorateId %{{[_0-9A-Za-z]*}} NodeMaxPayloadsAMDX [[MAXPAYLOADS]]
// CHECK: [[UINT:%[_0-9A-Za-z]*]] = OpTypeInt 32 0
// CHECK-DAG: [[U0:%[_0-9A-Za-z]*]] = OpConstant [[UINT]] 0
// CHECK-DAG: [[U1]] = OpConstant [[UINT]] 1
// CHECK-DAG: [[MAXPAYLOADS:%[_0-9A-Za-z]*]] = OpSpecConstant [[UINT]] 1
// CHECK-DAG: [[WGSIZEX:%[_0-9A-Za-z]*]] = OpSpecConstant [[UINT]] 1
// CHECK-DAG: [[SHADERINDEX:%[_0-9A-Za-z]*]] = OpSpecConstant [[UINT]] 0
// CHECK-DAG: [[NUMTHREADSX:%[_0-9A-Za-z]*]] = OpSpecConstant [[UINT]] 512

