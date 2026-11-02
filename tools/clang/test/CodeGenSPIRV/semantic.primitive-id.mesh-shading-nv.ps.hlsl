// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.1 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.1 -fcgl %s -spirv | FileCheck %s

// A fragment-stage PrimitiveId is enabled by MeshShadingNV as well as by
// Geometry (VUID-PrimitiveId-Fragment-04333). When the module already declares
// MeshShadingNV, Geometry must not be added.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability MeshShadingNV
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpExtension "SPV_NV_mesh_shader"

// CHECK:      OpEntryPoint Fragment %main "main"
// CHECK-SAME: %gl_PrimitiveID

// CHECK:      OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId

[[vk::ext_capability(/* MeshShadingNV */ 5266)]]
[[vk::ext_extension("SPV_NV_mesh_shader")]]
float4 main(uint id : SV_PrimitiveID) : SV_Target {
    return id;
}
