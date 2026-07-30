// RUN: %dxc -T ps_6_0 -E main -fcgl -Vd %s -spirv | FileCheck %s --implicit-check-not "OpDecorate %src_main"

// A function-level inline-SPIR-V decoration on an *entry point* is consumed by
// the stage-variable path and applied to the entry's interface variable, not to
// the source OpFunction. (An ordinary function is handled the other way -- the
// decoration lands on its OpFunction; see spv.intrinsicDecorate.function.hlsl.)
//
// The --implicit-check-not above asserts the source function (%src_main) is
// never decorated, which is what regressed when function-target handling was
// first added and was not caught because the check below matches any target.

// CHECK: OpDecorate %out_var_SV_Target Location 23

[[vk::ext_decorate(/* Location */ 30, 23)]]
float4 main() : SV_Target {
  return 1.0;
}
