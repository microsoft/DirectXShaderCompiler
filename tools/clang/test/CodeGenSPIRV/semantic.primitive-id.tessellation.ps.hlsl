// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// A fragment-stage PrimitiveId is enabled by Tessellation as well as by
// Geometry (VUID-PrimitiveId-Fragment-04333). When the module already declares
// Tessellation, Geometry must not be added.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability Tessellation
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpEntryPoint Fragment %main "main"
// CHECK-SAME: %gl_PrimitiveID

// CHECK:      OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId

[[vk::ext_capability(/* Tessellation */ 3)]]
float4 main(uint id : SV_PrimitiveID) : SV_Target {
    return id;
}
