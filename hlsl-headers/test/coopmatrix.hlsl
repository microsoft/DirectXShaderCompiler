// RUN-DISABLE: dxc -fspv-target-env=vulkan1.3 -T cs_6_2 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=int16_t %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=int %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=int64_t %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=uint %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=uint64_t %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=half %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=float %s | FileCheck %s
// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers -DTYPE=double %s | FileCheck %s

#include "vk/khr/cooperative_matrix.hlsli"

StructuredBuffer<float> structured_buffer;

RWStructuredBuffer<TYPE> data;


// CHECK: OpCapability CooperativeMatrixKHR
// CHECK: OpExtension "SPV_KHR_cooperative_matrix"


[[SPV_KHR_CooperativeMatrix]]
[numthreads(64, 1, 1)]
void main() {
  using CoopMat = vk::khr::CooperativeMatrix<
      TYPE, (int)spv::Scope::Subgroup, 16, 8,
      (int)vk::khr::internal::CooperativeMatrixUse::MatrixAKHR>;
  CoopMat m = CoopMat::LoadColumnMajor(data, 0);
  CoopMat n = CoopMat::LoadRowMajor(structured_buffer, 32);
  m = m + n;
  m = m - m;
  m = m.negate();
  m = m * 2.0;
  m.StoreRowMajor(data, 0);
  m.StoreColumnMajor(data, 16);
}
