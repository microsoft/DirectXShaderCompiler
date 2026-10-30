// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.1 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

// A declared ShaderViewportIndexLayerEXT enables a fragment-stage
// ViewportIndex on any target, so MultiViewport must not be added beside it.

// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpCapability ShaderViewportIndexLayerEXT
// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpExtension "SPV_EXT_shader_viewport_index_layer"
// CHECK:     OpEntryPoint Fragment %main "main"
// CHECK:     OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex

[[vk::ext_capability(/* ShaderViewportIndexLayerEXT */ 5254)]]
[[vk::ext_extension("SPV_EXT_shader_viewport_index_layer")]]
float4 main(uint input : SV_ViewportArrayIndex) : SV_Target {
    return input;
}
