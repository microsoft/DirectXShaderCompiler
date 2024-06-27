// RUN: dxc -fspv-target-env=vulkan1.3 -T cs_6_0 -E main -spirv -HV 2021 -I %hlsl_headers %s | FileCheck %s

#include "vk/khr/cooperative_matrix.hlsli"

RWStructuredBuffer<int> data;

// CHECK: OpCapability CooperativeMatrixKHR
// CHECK: OpExtension "SPV_KHR_cooperative_matrix"

// CHECK-DAG: [[typeA:%spirvIntrinsicType[_0-9]*]] = OpTypeCooperativeMatrixKHR %int %uint_3 %uint_16 %uint_4 %uint_0
// CHECK-DAG: [[typeB:%spirvIntrinsicType[_0-9]*]] = OpTypeCooperativeMatrixKHR %int %uint_3 %uint_4 %uint_8 %uint_1
// CHECK-DAG: [[typeAc:%spirvIntrinsicType[_0-9]*]] = OpTypeCooperativeMatrixKHR %int %uint_3 %uint_16 %uint_8 %uint_2

// CHECK: [[r:%[0-9]+]] = OpUndef [[typeAc]]
[numthreads(64, 1, 1)]
void main() {
  using IntMatA = vk::khr::CooperativeMatrixA<int, vk::SpvScopeSubgroup, 16, 4>;
  using IntMatB = vk::khr::CooperativeMatrixB<int, vk::SpvScopeSubgroup, 4, 8>;
  using IntMatAc = vk::khr::CooperativeMatrixAccumulator<int, vk::SpvScopeSubgroup, 16, 8>;

// CHECK: [[a:%[0-9]+]] = OpCooperativeMatrixLoadKHR [[typeA]] {{%[0-9]*}} %int_1
  IntMatA a = IntMatA::LoadColumnMajor(data, 0);

// CHECK: [[b:%[0-9]+]] = OpCooperativeMatrixLoadKHR [[typeB]] {{%[0-9]*}} %int_0
  IntMatB b = IntMatB::LoadRowMajor(data, 32);

  // TODO: Is default initialization meaningful?
  IntMatAc r;

// CHECK: [[r2:%[0-9]+]] = OpCooperativeMatrixMulAddKHR [[typeAc]] [[a]] [[b]] [[r]] MatrixASignedComponentsKHR|MatrixBSignedComponentsKHR|MatrixCSignedComponentsKHR|MatrixResultSignedComponentsKHR
  r = cooperativeMatrixMultiplyAdd(a, b, r);

// CHECK: [[r:%[0-9]+]] = OpCooperativeMatrixMulAddKHR [[typeAc]] [[a]] [[b]] [[r2]] MatrixASignedComponentsKHR|MatrixBSignedComponentsKHR|MatrixCSignedComponentsKHR|MatrixResultSignedComponentsKHR|SaturatingAccumulationKHR
  r = cooperativeMatrixSaturatingMultiplyAdd(a, b, r);

// CHECK: OpCooperativeMatrixStoreKHR {{.*}} [[r]] %int_0
  r.StoreRowMajor(data, 64);
}
