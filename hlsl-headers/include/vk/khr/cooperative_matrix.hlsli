// Copyright (c) 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef VULKAN_HLSL_SPV_KHR_COOPERATIVE_MATRIX_H_
#define VULKAN_HLSL_SPV_KHR_COOPERATIVE_MATRIX_H_


// TODO: Add a macro to HLSL to be able to check the Vulkan version being
// targeted.

#define SPV_KHR_CooperativeMatrix                  \
  vk::ext_extension("SPV_KHR_cooperative_matrix"), \
      vk::ext_capability(/* CooperativeMatrixKHRCapability */ 6022)

namespace vk {
namespace khr {

template <typename ComponentType, uint scope, uint rows, uint columns, uint use>
class CooperativeMatrix {
  CooperativeMatrix negate();
  CooperativeMatrix operator+(CooperativeMatrix other);
  CooperativeMatrix operator-(CooperativeMatrix other);
  CooperativeMatrix operator*(ComponentType scalar);

  void StoreRowMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);
  void StoreColumnMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);

  template <class BufferType>
  static CooperativeMatrix LoadRowMajor(BufferType buffer, uint32_t index);

  template <class BufferType>
  static CooperativeMatrix LoadColumnMajor(BufferType buffer, uint32_t index);

  static uint32_t GetLength();
};

template <typename ComponentType, uint scope, uint rows, uint columns>
using CooperativeMatrixA = CooperativeMatrix<ComponentType, scope, rows, columns, 0>;

template <typename ComponentType, uint scope, uint rows, uint columns>
using CooperativeMatrixB = CooperativeMatrix<ComponentType, scope, rows, columns, 1>;

template <typename ComponentType, uint scope, uint rows, uint columns>
using CooperativeMatrixAccumulator = CooperativeMatrix<ComponentType, scope, rows, columns, 2>;

template <typename ComponentType, uint scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
    cooperativeMatrixMultiplyAdd(CooperativeMatrixA<ComponentType, scope, rows, K> a,
                 CooperativeMatrixB<ComponentType, scope, K, columns> b,
                 CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

template <typename ComponentType, uint scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
    cooperativeMatrixSaturatingMultiplyAdd(CooperativeMatrixA<ComponentType, scope, rows, K> a,
                 CooperativeMatrixB<ComponentType, scope, K, columns> b,
                 CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

}  // namespace khr
}  // namespace spv

#include "cooperative_matrix.impl"
#endif  // VULKAN_HLSL_SPV_KHR_COOPERATIVE_MATRIX_H_
