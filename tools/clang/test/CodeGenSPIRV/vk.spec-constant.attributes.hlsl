// RUN: %dxc -spirv -Vd -Od -T lib_6_8 -fspv-target-env=vulkan1.3 -DSPEC=1 %s | FileCheck %s
// RUN: not %dxc -spirv -Vd -Od -T lib_6_8 -fspv-target-env=vulkan1.3 %s 2>&1 | FileCheck --check-prefix=NOSPEC %s

// Note: validation disabled until NodePayloadAMDX pointers are allowed
// as function arguments

struct InputPayload {
  uint grid : SV_DispatchGrid;
};

struct OutputPayload {
  uint foo;
};

#ifdef SPEC
[[vk::constant_id(0)]] const
#endif
uint MaxPayloads = 1;
#ifdef SPEC
[[vk::constant_id(1)]] const
#endif
uint WorkgroupSizeX = 1;
#ifdef SPEC
[[vk::constant_id(2)]] const
#endif
uint ShaderIndex = 0;
#ifdef SPEC
[[vk::constant_id(3)]] const
#endif
uint NumThreadsX = 512;

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

// NOSPEC-DAG: error: 'MaxRecords' attribute requires an integer constant
// NOSPEC-DAG: error: 'NodeID' attribute requires an integer constant
// NOSPEC-DAG: error: 'NodeDispatchGrid' attribute requires an integer constant
// NOSPEC-DAG: error: 'NumThreads' attribute requires an integer constant

