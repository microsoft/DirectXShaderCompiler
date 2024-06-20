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

#ifndef _HLSL_VK_KHR_COOPERATIVE_MATRIX_H_
#define _HLSL_VK_KHR_COOPERATIVE_MATRIX_H_

// TODO: Add a macro to HLSL to be able to check the Vulkan version being
// targeted.

#include "vk/spirv.h"

namespace vk {
namespace khr {

template <typename ComponentType, Scope scope, uint rows, uint columns,
          CooperativeMatrixUse use>
class CooperativeMatrix {
  CooperativeMatrix negate();
  CooperativeMatrix operator+(CooperativeMatrix other);
  CooperativeMatrix operator-(CooperativeMatrix other);
  CooperativeMatrix operator*(CooperativeMatrix other);
  CooperativeMatrix operator/(CooperativeMatrix other);
  CooperativeMatrix operator*(ComponentType scalar);

  void StoreRowMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);
  void StoreColumnMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);
  void StoreRowMajor(RWStructuredBuffer<ComponentType> data, uint32_t index,
                     uint32_t stride, MemoryAccessMask memoryAccessMask);
  void StoreColumnMajor(RWStructuredBuffer<ComponentType> data, uint32_t index,
                        uint32_t stride, MemoryAccessMask memoryAccessMask);

  template <class BufferType>
  static CooperativeMatrix LoadRowMajor(BufferType buffer, uint32_t index);

  template <class BufferType>
  static CooperativeMatrix LoadColumnMajor(BufferType buffer, uint32_t index);

  template <class BufferType>
  static CooperativeMatrix
  LoadRowMajor(BufferType buffer, uint32_t index, uint32_t stride,
               MemoryAccessMask memoryAccessMask = MemoryAccessMaskNone);

  template <class BufferType>
  static CooperativeMatrix
  LoadColumnMajor(BufferType buffer, uint32_t index, uint32_t stride,
                  MemoryAccessMask memoryAccessMask = MemoryAccessMaskNone);

  static uint32_t GetLength();

  static const bool hasSignedIntegerComponentType =
      (ComponentType(0) - ComponentType(1) < ComponentType(0));

  // clang-format off
  using SpirvMatrixType = vk::SpirvOpaqueType<
      /* OpTypeCooperativeMatrixKHR */ 4456, ComponentType,
      vk::integral_constant<uint, scope>, vk::integral_constant<uint, rows>,
      vk::integral_constant<uint, columns>, vk::integral_constant<uint, use> >;
  // pragma clang-format on

  [[vk::ext_extension("SPV_KHR_cooperative_matrix")]] [[vk::ext_capability(
      /* CooperativeMatrixKHRCapability */ 6022)]] SpirvMatrixType _matrix;
};

template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixA =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixAKHR>;

template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixB =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixBKHR>;

template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixAccumulator =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixAccumulatorKHR>;

template <typename ComponentType, Scope scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
cooperativeMatrixMultiplyAdd(
    CooperativeMatrixA<ComponentType, scope, rows, K> a,
    CooperativeMatrixB<ComponentType, scope, K, columns> b,
    CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

template <typename ComponentType, Scope scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
cooperativeMatrixSaturatingMultiplyAdd(
    CooperativeMatrixA<ComponentType, scope, rows, K> a,
    CooperativeMatrixB<ComponentType, scope, K, columns> b,
    CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

} // namespace khr
} // namespace vk

#include "cooperative_matrix.impl"
#endif // _HLSL_VK_KHR_COOPERATIVE_MATRIX_H_
