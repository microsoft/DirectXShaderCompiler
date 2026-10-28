// RUN: %dxc -T ps_6_0 -E main -fspv-debug=line -spirv %s | FileCheck %s
// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan-with-source -spirv %s | FileCheck %s

// Verifies: arithmetic on a vk::constant_id value compiles with debug info.
//
// OpSpecConstantOp is a module-scope instruction. It is visited before the
// debug sources and the functions, so no line information may be emitted for
// it. With Vulkan debug info a DebugLine would reference a source id that has
// not been assigned yet, and compilation fails.

// CHECK: [[mul:%[0-9]+]] = OpSpecConstantOp %int IMul %n %int_2
// CHECK:                   OpIAdd %int [[mul]] %int_1

[[vk::constant_id(1)]] const int n = 4;

float4 main() : SV_Target {
  int m = n * 2 + 1;
  return float4(m, 0, 0, 1);
}
