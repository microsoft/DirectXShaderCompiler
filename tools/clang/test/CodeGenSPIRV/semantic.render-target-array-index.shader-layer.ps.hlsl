// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=vulkan1.2 %s -spirv | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-target-env=universal1.5 %s -spirv | FileCheck %s

// A declared ShaderLayer enables a fragment-stage Layer, so Geometry must not
// be added beside it. ShaderLayer needs SPIR-V 1.5, which both targets provide.

// CHECK-NOT: OpCapability Geometry
// CHECK:     OpCapability ShaderLayer
// CHECK-NOT: OpCapability Geometry
// CHECK:     OpEntryPoint Fragment %main "main"
// CHECK:     OpDecorate %gl_Layer BuiltIn Layer

[[vk::ext_capability(/* ShaderLayer */ 69)]]
float4 main(uint input : SV_RenderTargetArrayIndex) : SV_Target {
    return input;
}
