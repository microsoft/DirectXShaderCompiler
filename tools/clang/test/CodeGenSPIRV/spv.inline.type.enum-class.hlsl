// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

enum class Scope {
  CrossDevice = 0,
  Device = 1,
  Workgroup = 2,
  Subgroup = 3,
  Invocation = 4,
  QueueFamily = 5,
  QueueFamilyKHR = 5,
  ShaderCallKHR = 6,
};

enum class CooperativeMatrixUse {
  MatrixAKHR = 0,
  MatrixBKHR = 1,
  MatrixAccumulatorKHR = 2,
};

// CHECK: %spirvIntrinsicType = OpTypeCooperativeMatrixKHR %float %int_3 %int_32 %int_32 %int_0
typedef vk::SpirvOpaqueType</* OpTypeCooperativeMatrixKHR */ 4456, float, Scope::Subgroup, 32, 32, CooperativeMatrixUse::MatrixAKHR> mat_t;

[[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
[[vk::ext_capability(/* CooperativeMatrixKHR */ 6022)]]
void main() {
  // CHECK: %mat = OpVariable %_ptr_Function_spirvIntrinsicType
  mat_t mat;
}
