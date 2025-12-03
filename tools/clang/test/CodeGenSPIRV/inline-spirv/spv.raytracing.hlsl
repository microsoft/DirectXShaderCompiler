// RUN: %dxc %s -T cs_6_8 -spirv -fspv-target-env=vulkan1.3 -E main -O0 | Filecheck %s

// CHECK-DAG: OpCapability RuntimeDescriptorArray
// CHECK-DAG: OpCapability RayQueryKHR

// CHECK-DAG: OpDecorate %MyScene DescriptorSet 1
// CHECK-DAG: OpDecorate %MyScene Binding 3

using A = vk::SpirvOpaqueType</* OpTypeAccelerationStructureKHR */ 5341>;
// CHECK: %[[name:[^ ]+]] = OpTypeAccelerationStructureKHR

using RA [[vk::ext_capability(/* RuntimeDescriptorArray */ 5302)]] = vk::SpirvOpaqueType</* OpTypeRuntimeArray */ 29, A>;
// CHECK: %[[rarr:[^ ]+]] = OpTypeRuntimeArray %[[name]]

// CHECK: %[[ptr:[^ ]+]] = OpTypePointer UniformConstant %[[rarr]]
// CHECK: %MyScene = OpVariable %[[ptr]] UniformConstant
[[vk::binding(3, 1)]]
[[vk::ext_storage_class(0)]]
RA MyScene;

[numthreads(1, 1, 1)]
void main() {}
