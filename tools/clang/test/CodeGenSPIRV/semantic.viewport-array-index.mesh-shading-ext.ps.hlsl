// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// A fragment-stage ViewportIndex is enabled by MeshShadingEXT as well as by
// MultiViewport. When the module already declares MeshShadingEXT,
// MultiViewport, which depends on Geometry, must not be added.

// CHECK-NOT: OpCapability MultiViewport
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability MeshShadingEXT
// CHECK-NOT: OpCapability MultiViewport
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpExtension "SPV_EXT_mesh_shader"

// CHECK:      OpEntryPoint Fragment %main "main"
// CHECK-SAME: %gl_ViewportIndex

// CHECK:      OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex

[[vk::ext_capability(/* MeshShadingEXT */ 5283)]]
[[vk::ext_extension("SPV_EXT_mesh_shader")]]
float4 main(uint input : SV_ViewportArrayIndex) : SV_Target {
    return input;
}
