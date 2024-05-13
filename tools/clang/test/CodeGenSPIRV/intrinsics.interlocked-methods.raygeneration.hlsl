// RUN: %dxc -fspv-target-env=vulkan1.2 -T lib_6_4 -E main -spirv -O0 %s | FileCheck %s

RWStructuredBuffer<int> g_buff;

// CHECK-DAG: OpCapability VulkanMemoryModel
// CHECK-DAG: OpCapability VulkanMemoryModelDeviceScope
// CHECK:     OpMemoryModel Logical Vulkan

[shader("raygeneration")]
void main()
{
// CHECK: OpAtomicIAdd %int {{%[0-9]+}} %uint_1
//                                      1 = Device scope
    InterlockedAdd(g_buff[0], WaveGetLaneCount());
}
