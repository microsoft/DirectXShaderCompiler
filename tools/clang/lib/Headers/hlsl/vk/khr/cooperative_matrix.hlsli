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

namespace vk {

// TODO: Move these defines to a new header file for defines.

typedef enum SpvCooperativeMatrixUse_ {
    SpvCooperativeMatrixUseMatrixAKHR = 0,
    SpvCooperativeMatrixUseMatrixBKHR = 1,
    SpvCooperativeMatrixUseMatrixAccumulatorKHR = 2,
    SpvCooperativeMatrixUseMax = 0x7fffffff,
} SpvCooperativeMatrixUse;

typedef enum SpvCooperativeMatrixLayout_ {
    SpvCooperativeMatrixLayoutRowMajorKHR = 0,
    SpvCooperativeMatrixLayoutColumnMajorKHR = 1,
    SpvCooperativeMatrixLayoutRowBlockedInterleavedARM = 4202,
    SpvCooperativeMatrixLayoutColumnBlockedInterleavedARM = 4203,
    SpvCooperativeMatrixLayoutMax = 0x7fffffff,
} SpvCooperativeMatrixLayout;

typedef enum SpvMemoryAccessMask_ {
    SpvMemoryAccessMaskNone = 0,
    SpvMemoryAccessVolatileMask = 0x00000001,
    SpvMemoryAccessAlignedMask = 0x00000002,
    SpvMemoryAccessNontemporalMask = 0x00000004,
    SpvMemoryAccessMakePointerAvailableMask = 0x00000008,
    SpvMemoryAccessMakePointerAvailableKHRMask = 0x00000008,
    SpvMemoryAccessMakePointerVisibleMask = 0x00000010,
    SpvMemoryAccessMakePointerVisibleKHRMask = 0x00000010,
    SpvMemoryAccessNonPrivatePointerMask = 0x00000020,
    SpvMemoryAccessNonPrivatePointerKHRMask = 0x00000020,
    SpvMemoryAccessAliasScopeINTELMaskMask = 0x00010000,
    SpvMemoryAccessNoAliasINTELMaskMask = 0x00020000,
} SpvMemoryAccessMask;

typedef enum SpvCooperativeMatrixOperandsMask_ {
    SpvCooperativeMatrixOperandsMaskNone = 0,
    SpvCooperativeMatrixOperandsMatrixASignedComponentsKHRMask = 0x00000001,
    SpvCooperativeMatrixOperandsMatrixBSignedComponentsKHRMask = 0x00000002,
    SpvCooperativeMatrixOperandsMatrixCSignedComponentsKHRMask = 0x00000004,
    SpvCooperativeMatrixOperandsMatrixResultSignedComponentsKHRMask = 0x00000008,
    SpvCooperativeMatrixOperandsSaturatingAccumulationKHRMask = 0x00000010,
} SpvCooperativeMatrixOperandsMask;

typedef enum SpvScope_ {
    SpvScopeCrossDevice = 0,
    SpvScopeDevice = 1,
    SpvScopeWorkgroup = 2,
    SpvScopeSubgroup = 3,
    SpvScopeInvocation = 4,
    SpvScopeQueueFamily = 5,
    SpvScopeQueueFamilyKHR = 5,
    SpvScopeShaderCallKHR = 6,
    SpvScopeMax = 0x7fffffff,
} SpvScope;

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
using CooperativeMatrixA =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      SpvCooperativeMatrixUseMatrixAKHR>;

template <typename ComponentType, uint scope, uint rows, uint columns>
using CooperativeMatrixB =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      SpvCooperativeMatrixUseMatrixBKHR>;

template <typename ComponentType, uint scope, uint rows, uint columns>
using CooperativeMatrixAccumulator =
    CooperativeMatrix<ComponentType, scope, rows, columns,
                      SpvCooperativeMatrixUseMatrixAccumulatorKHR>;

template <typename ComponentType, uint scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
cooperativeMatrixMultiplyAdd(
    CooperativeMatrixA<ComponentType, scope, rows, K> a,
    CooperativeMatrixB<ComponentType, scope, K, columns> b,
    CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

template <typename ComponentType, uint scope, uint rows, uint columns, uint K>
CooperativeMatrixAccumulator<ComponentType, scope, rows, columns>
cooperativeMatrixSaturatingMultiplyAdd(
    CooperativeMatrixA<ComponentType, scope, rows, K> a,
    CooperativeMatrixB<ComponentType, scope, K, columns> b,
    CooperativeMatrixAccumulator<ComponentType, scope, rows, columns> c);

} // namespace khr
} // namespace vk

#include "cooperative_matrix.impl"
#endif // VULKAN_HLSL_SPV_KHR_COOPERATIVE_MATRIX_H_
