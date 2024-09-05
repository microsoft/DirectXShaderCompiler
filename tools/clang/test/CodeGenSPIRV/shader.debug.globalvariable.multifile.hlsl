// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan -fcgl %s -spirv | FileCheck %s

// CHECK: [[set:%[0-9]+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK-DAG: [[from_include_name:%[0-9]+]] = OpString "fromInclude"
// CHECK-DAG: [[from_main_name:%[0-9]+]] = OpString "fromMain"
// CHECK-DAG: [[file_hlsli:%[0-9]+]] = OpString "{{.*}}shader.debug.globalvariable.multifile.hlsli"
// CHECK-DAG: [[file_hlsl:%[0-9]+]] = OpString "{{.*}}shader.debug.globalvariable.multifile.hlsl"
// CHECK-DAG: [[src_hlsli:%[0-9]+]] = OpExtInst %void [[set]] DebugSource [[file_hlsli]]
// CHECK-DAG: [[src_hlsl:%[0-9]+]] = OpExtInst %void [[set]] DebugSource [[file_hlsl]]
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugTypeMember [[from_include_name]] {{%[0-9]+}} [[src_hlsli]]
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugTypeMember [[from_main_name]] {{%[0-9]+}} [[src_hlsl]]

#include "shader.debug.globalvariable.multifile.hlsli"

uniform float fromMain;

float4 main(float4 color : COLOR) : SV_TARGET {
  return color + (fromInclude + fromMain).xxxx;
}
