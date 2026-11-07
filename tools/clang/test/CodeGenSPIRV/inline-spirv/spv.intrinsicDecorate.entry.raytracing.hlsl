// RUN: %dxc -T lib_6_3 -fspv-target-env=vulkan1.2 -fcgl -Vd %s -spirv | FileCheck %s

// Ray-tracing entries skip processInlineSpirvAttributes, so check that their
// capability and extension are still emitted.

// CHECK-DAG: OpCapability Int8
// CHECK-DAG: OpExtension "some_extension"

[[vk::ext_capability(/* Int8 */ 39)]]
[[vk::ext_extension("some_extension")]]
[shader("raygeneration")]
void main() {}
