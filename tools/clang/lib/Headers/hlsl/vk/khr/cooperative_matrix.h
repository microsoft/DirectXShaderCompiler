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

// The base cooperative matrix class. The template arguments correspond to the
// operands in the OpTypeCooperativeMatrixKHR instruction.
template <typename ComponentType, Scope scope, uint rows, uint columns,
          CooperativeMatrixUse use>
class CooperativeMatrix {
  template <class NewComponentType>
  CooperativeMatrix<NewComponentType, scope, rows, columns, use> cast();

  // Apply OpSNegate or OFNegate, depending on ComponentType, in a element by
  // element manner.
  CooperativeMatrix negate();

  // Apply OpIAdd or OFAdd, depending on ComponentType, in a element by element
  // manner.
  CooperativeMatrix operator+(CooperativeMatrix other);

  // Apply OpISub or OFSub, depending on ComponentType, in a element by element
  // manner.
  CooperativeMatrix operator-(CooperativeMatrix other);

  // Apply OpIMul or OFMul, depending on ComponentType, in a element by element
  // manner.
  CooperativeMatrix operator*(CooperativeMatrix other);

  // Apply OpSDiv, OpUDiv or OFDiv, depending on ComponentType, in a element by
  // element manner.
  CooperativeMatrix operator/(CooperativeMatrix other);

  // Apply OpMatrixTimesScalar in a element by element manner.
  CooperativeMatrix operator*(ComponentType scalar);

  // Store the cooperative matrix using OpCooperativeMatrixStoreKHR to data[i]
  // using memory layout RowMajorKHR.￼
  void StoreRowMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);

  // Store the cooperative matrix using OpCooperativeMatrixStoreKHR to data[i]
  // using memory layout ColumnMajorKHR.￼
  void StoreColumnMajor(RWStructuredBuffer<ComponentType> data, uint32_t index);

  // Store the cooperative matrix using OpCooperativeMatrixStoreKHR to data[i]
  // using memory layout RowMajorKHR and the given stride and memory access
  // mask.
  void StoreRowMajor(RWStructuredBuffer<ComponentType> data, uint32_t index,
                     uint32_t stride, MemoryAccessMask memoryAccessMask);

  // Store the cooperative matrix using OpCooperativeMatrixStoreKHR to data[i]
  // using memory layout ColumnMajorKHR and the given stride and memory access
  // mask.
  void StoreColumnMajor(RWStructuredBuffer<ComponentType> data, uint32_t index,
                        uint32_t stride, MemoryAccessMask memoryAccessMask);

  // Constructs a cooperative matrix with all values initialized to v. Note that
  // all active threads must have the same value for v.
  static CooperativeMatrix splat(ComponentType v);

  // Load the cooperative matrix using OpCooperativeMatrixLoadKHR from data[i]
  // using memory layout RowMajorKHR.
  template <class BufferType>
  static CooperativeMatrix LoadRowMajor(BufferType buffer, uint32_t index);

  // Loads a cooperative matrix using OpCooperativeMatrixLoadKHR from data[i]
  // using memory layout ColumnMajorKHR.￼
  template <class BufferType>
  static CooperativeMatrix LoadColumnMajor(BufferType buffer, uint32_t index);

  // Loads a cooperative matrix using OpCooperativeMatrixLoadKHR from data[i]
  // using memory layout RowMajorKHR, and the given stride and memory access
  // mask.
  template <class BufferType>
  static CooperativeMatrix
  LoadRowMajor(BufferType buffer, uint32_t index, uint32_t stride,
               MemoryAccessMask memoryAccessMask = MemoryAccessMaskNone);

  // Loads a cooperative matrix using OpCooperativeMatrixLoadKHR from data[i]
  // using memory layout ColumnMajorKHR, and the given stride and memory access
  // mask.
  template <class BufferType>
  static CooperativeMatrix
  LoadColumnMajor(BufferType buffer, uint32_t index, uint32_t stride,
                  MemoryAccessMask memoryAccessMask = MemoryAccessMaskNone);

  // Returns the result of OpCooperativeMatrixLengthKHR on the current type.￼
  static uint32_t GetLength();

  // Functions to access the elements of the cooperative matrix. The index must
  // be less than GetLength().
  void Set(ComponentType value, uint32_t index);
  ComponentType Get(uint32_t index);

  static const bool hasSignedIntegerComponentType =
      (ComponentType(0) - ComponentType(1) < ComponentType(0));

  // clang-format off
  using SpirvMatrixType = vk::SpirvOpaqueType<
      /* OpTypeCooperativeMatrixKHR */ 4456, ComponentType,
      vk::integral_constant<uint, scope>, vk::integral_constant<uint, rows>,
      vk::integral_constant<uint, columns>, vk::integral_constant<uint, use> >;

  [[vk::ext_extension("SPV_KHR_cooperative_matrix")]]
  [[vk::ext_capability(/* CooperativeMatrixKHRCapability */ 6022)]]
  SpirvMatrixType _matrix;
  // clang-format on
};

// Cooperative matrix that can be used in the "a" position of a multiple add
// instruction (r = (a * b) + c).
template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixA =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixAKHR>;

// Cooperative matrix that can be used in the "b" position of a multiple add
// instruction (r = (a * b) + c).
template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixB =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixBKHR>;

// Cooperative matrix that can be used in the "r" and "c" position of a multiple
// add instruction (r = (a * b) + c).
template <typename ComponentType, Scope scope, uint rows, uint columns>
using CooperativeMatrixAccumulator =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      CooperativeMatrixUseMatrixAccumulatorKHR>;

// Returns the result of OpCooperativeMatrixMulAddKHR when applied to a, b, and
// c. The cooperative matrix operands are inferred, with the
// SaturatingAccumulationKHR bit not set.
template <typename ComponentType, Scope scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
cooperativeMatrixMultiplyAdd(
    CooperativeMatrixA<ComponentType, scope, rows, K> a,
    CooperativeMatrixB<ComponentType, scope, K, columns> b,
    CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

// Returns the result of OpCooperativeMatrixMulAddKHR when applied to a, b, and
// c. The cooperative matrix operands are inferred, with the
// SaturatingAccumulationKHR bit set.
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
