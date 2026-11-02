// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// A fragment-stage Layer is enabled by MeshShadingEXT as well as by Geometry.
// When the module already declares MeshShadingEXT, Geometry must not be added.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability MeshShadingEXT
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpExtension "SPV_EXT_mesh_shader"

// CHECK:      OpEntryPoint Fragment %main "main"
// CHECK-SAME: %gl_Layer

// CHECK:      OpDecorate %gl_Layer BuiltIn Layer

[[vk::ext_capability(/* MeshShadingEXT */ 5283)]]
[[vk::ext_extension("SPV_EXT_mesh_shader")]]
float4 main(uint input : SV_RenderTargetArrayIndex) : SV_Target {
    return input;
}
