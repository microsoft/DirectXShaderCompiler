// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers %s | FileCheck %s

#include "vk/khr/cooperative_matrix.h"

RWStructuredBuffer<int> data;

groupshared float shared_data[64];

// CHECK: OpCapability CooperativeMatrixKHR
// CHECK: OpExtension "SPV_KHR_cooperative_matrix"
[numthreads(64, 1, 1)] void main() {
  using FloatMatA = vk::khr::CooperativeMatrixA<float, vk::ScopeSubgroup, 16, 4>;


// CHECK: [[ac:%[0-9]+]] = OpAccessChain %_ptr_StorageBuffer_int %data %int_0 %uint_0
// CHECK: [[ld:%[0-9]+]] = OpCooperativeMatrixLoadKHR %spirvIntrinsicType [[ac]] %int_1
  FloatMatA m = FloatMatA::LoadColumnMajor(data, 0);

// CHECK: [[ac:%[0-9]+]] = OpAccessChain %_ptr_Workgroup_float %shared_data %int_0
// CHECK: OpCooperativeMatrixStoreKHR [[ac]] [[ld]] %int_1
  __builtin_spv_CooperativeMatrixStoreKHR(shared_data[0], m._matrix,
                                          vk::CooperativeMatrixLayoutColumnMajorKHR);

  FloatMatA m2;
// CHECK: [[ld:%[0-9]+]] = OpCooperativeMatrixLoadKHR %spirvIntrinsicType [[ac]] %int_1
  m2._matrix = __builtin_spv_CooperativeMatrixLoadKHR<FloatMatA::SpirvMatrixType>(shared_data[0], vk::CooperativeMatrixLayoutColumnMajorKHR);

// CHECK: [[ac:%[0-9]+]] = OpAccessChain %_ptr_StorageBuffer_int %data %int_0 %uint_64
// CHECK: OpCooperativeMatrixStoreKHR [[ac]] [[ld]] %int_0
  m2.StoreRowMajor(data, 64);
}
