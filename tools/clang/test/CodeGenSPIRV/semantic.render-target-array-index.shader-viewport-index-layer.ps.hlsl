// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.1 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

// A declared ShaderViewportIndexLayerEXT enables a fragment-stage Layer on any
// target, so Geometry must not be added beside it.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability ShaderViewportIndexLayerEXT
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpExtension "SPV_EXT_shader_viewport_index_layer"
// CHECK:     OpEntryPoint Fragment %main "main"
// CHECK:     OpDecorate %gl_Layer BuiltIn Layer

[[vk::ext_capability(/* ShaderViewportIndexLayerEXT */ 5254)]]
[[vk::ext_extension("SPV_EXT_shader_viewport_index_layer")]]
float4 main(uint input : SV_RenderTargetArrayIndex) : SV_Target {
    return input;
}
