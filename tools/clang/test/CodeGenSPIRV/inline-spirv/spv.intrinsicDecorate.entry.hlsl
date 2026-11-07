// RUN: %dxc -T ps_6_0 -E main -fcgl -Vd %s -spirv | FileCheck %s --implicit-check-not "OpDecorate %src_main"

// [[vk::ext_decorate]] on an entry point decorates its interface variable, not
// the OpFunction.

// CHECK: OpDecorate %out_var_SV_Target Location 23

[[vk::ext_decorate(/* Location */ 30, 23)]]
float4 main() : SV_Target {
  return 1.0;
}
