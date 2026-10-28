// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -DCAPABILITY_ON_INSTRUCTION %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -DCAPABILITY_ON_INSTRUCTION -fcgl %s -spirv | FileCheck %s

// A fragment-stage PrimitiveId is enabled by MeshShadingEXT as well as by
// Geometry (VUID-PrimitiveId-Fragment-04333). When the module already declares
// MeshShadingEXT, Geometry must not be added, so the module stays valid on a
// device without geometry shaders. The capability on the entry point is added
// while the module is emitted. The one on a vk::ext_instruction function is
// added only when the function bodies are visited, after the PrimitiveId
// decoration.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability MeshShadingEXT
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpExtension "SPV_EXT_mesh_shader"

// CHECK:      OpEntryPoint Fragment %main "main"
// CHECK-SAME: %gl_PrimitiveID

// CHECK:      OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId

#ifdef CAPABILITY_ON_INSTRUCTION
[[vk::ext_capability(/* MeshShadingEXT */ 5283)]]
[[vk::ext_extension("SPV_EXT_mesh_shader")]]
[[vk::ext_instruction(/* OpCopyObject */ 83)]]
uint copyId(uint x);

float4 main(uint id : SV_PrimitiveID) : SV_Target {
    return copyId(id);
}
#else
[[vk::ext_capability(/* MeshShadingEXT */ 5283)]]
[[vk::ext_extension("SPV_EXT_mesh_shader")]]
float4 main(uint id : SV_PrimitiveID) : SV_Target {
    return id;
}
#endif
