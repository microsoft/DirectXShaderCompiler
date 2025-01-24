// RUN: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.3 -fcgl  %s -spirv | FileCheck %s

// CHECK: OpEntryPoint GLCompute %main "main"
// CHECK: OpExecutionModeId %main LocalSizeId [[spec_constant:%[0-9a-zA-Z]+]] %int_8 %int_1
// CHECK: [[spec_constant]] = OpSpecConstant %uint 8

[[vk::constant_id(1)]] const uint specConstant = 8;
[[vk::LocalSizeId(specConstant, 8, 1)]]
void main() {}
