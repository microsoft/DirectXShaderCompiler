// RUN: %dxc -T lib_6_3 -fspv-target-env=vulkan1.2 -fcgl -Vd %s -spirv | FileCheck %s

// A ray-tracing entry point returns from emitEntryFunctionWrapper (via the
// isRay() path) before processInlineSpirvAttributes runs, so unlike other entry
// stages its [[vk::ext_capability]]/[[vk::ext_extension]] are registered only by
// the general function path in doFunctionDecl. This guards that they are still
// emitted for ray-tracing entries.

// CHECK-DAG: OpCapability Int8
// CHECK-DAG: OpExtension "some_extension"

[[vk::ext_capability(/* Int8 */ 39)]]
[[vk::ext_extension("some_extension")]]
[shader("raygeneration")]
void main() {}
