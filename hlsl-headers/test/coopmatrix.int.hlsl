// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers %s | FileCheck %s

#include "vk/khr/cooperative_matrix.hlsli"

RWStructuredBuffer<float> data;
StructuredBuffer<float> structured_buffer;

RWStructuredBuffer<int> int_data;


// CHECK: OpCapability CooperativeMatrixKHR
// CHECK: OpExtension "SPV_KHR_cooperative_matrix"


[[SPV_KHR_CooperativeMatrix]]
[numthreads(64, 1, 1)]
void main() {
  using IntCoopMat = vk::khr::CooperativeMatrix<
      int, (int)spv::Scope::Subgroup, 16, 8,
      (int)vk::khr::internal::CooperativeMatrixUse::MatrixAKHR>;
  IntCoopMat m = IntCoopMat::LoadColumnMajor(int_data, 0);
  IntCoopMat n = IntCoopMat::LoadRowMajor(structured_buffer, 32);
  m = m + n;
  m = m - m;
  m = m.negate();
  m = m * 2.0;
  m.StoreRowMajor(int_data, 0);
  m.StoreColumnMajor(int_data, 16);
}
