// RUN: %dxc %s -T cs_6_8 -spirv -fspv-target-env=vulkan1.3 -E main -O0 | FileCheck %s

// CHECK-DAG: OpCapability RuntimeDescriptorArray
// CHECK-DAG: OpCapability RayQueryKHR
// CHECK-DAG: OpDecorate %MyScene DescriptorSet 3
// CHECK-DAG: OpDecorate %MyScene Binding 2

using A = vk::SpirvOpaqueType</* OpTypeAccelerationStructureKHR */ 5341>;
// CHECK: %[[name:[^ ]+]] = OpTypeAccelerationStructureKHR

using RA [[vk::ext_capability(/* RuntimeDescriptorArray */ 5302)]] = vk::SpirvOpaqueType</* OpTypeRuntimeArray */ 29, A>;
// CHECK: %[[rarr:[^ ]+]] = OpTypeRuntimeArray %[[name]]

// CHECK: %[[ptr:[^ ]+]] = OpTypePointer UniformConstant %[[rarr]]
// CHECK: %MyScene = OpVariable %[[ptr]] UniformConstant
[[vk::ext_storage_class(0)]]
[[vk::ext_decorate(/* Binding */ 33, 2)]]
[[vk::ext_decorate(/* DescriptorSet */ 34, 3)]]
RA MyScene;

[numthreads(1, 1, 1)]
void main() {}
