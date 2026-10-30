// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.2 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=universal1.5 %s -spirv | FileCheck %s

// A declared ShaderViewportIndex enables a fragment-stage ViewportIndex, so
// MultiViewport must not be added beside it. ShaderViewportIndex needs
// SPIR-V 1.5, which both targets provide.

// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpCapability ShaderViewportIndex
// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpEntryPoint Fragment %main "main"
// CHECK:     OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex

[[vk::ext_capability(/* ShaderViewportIndex */ 70)]]
float4 main(uint input : SV_ViewportArrayIndex) : SV_Target {
    return input;
}
