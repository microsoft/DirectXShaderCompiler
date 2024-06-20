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

#ifndef VULKAN_HLSL_COMMON_H_
#define VULKAN_HLSL_COMMON_H_

// TODO: See if spirv.h can be used instead of this file.

namespace spv {
enum class MemoryOperand {
  None = 0x0,
  Volatile = 0x1,
  Aligned = 0x2,
  Nontemporal = 0x4,
  MakePointerAvailable = 0x8,
  MakePointerAvailableKHR = 0x8,
  MakePointerVisible = 0x10,
  MakePointerVisibleKHR = 0x10,
  NonPrivatePointer = 0x20,
  NonPrivatePointerKHR = 0x20,
  AliasScopeINTELMask = 0x10000,
  NoAliasINTELMask = 0x20000,
};

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

enum class StorageClass { Function = 7, StorageBuffer = 12 };

#define DECLARE_UNARY_OP(name, opcode) \
  template <typename ResultType>       \
  [[vk::ext_instruction(opcode)]]      \
  ResultType name(ResultType a)

DECLARE_UNARY_OP(SNegate, 126);
DECLARE_UNARY_OP(FNegate, 127);

#undef DECLARY_UNARY_OP

#define DECLARE_BINOP(name, opcode) \
  template <typename ResultType>    \
  [[vk::ext_instruction(opcode)]]   \
  ResultType name(ResultType a, ResultType b)

DECLARE_BINOP(IAdd, 128);
DECLARE_BINOP(FAdd, 129);
DECLARE_BINOP(ISub, 130);
DECLARE_BINOP(FSub, 131);
DECLARE_BINOP(IMul, 132);
DECLARE_BINOP(FMul, 133);
DECLARE_BINOP(UDiv, 134);
DECLARE_BINOP(SDiv, 135);
DECLARE_BINOP(FDiv, 136);

#undef DECLARE_BINOP

template <typename ResultType, typename ComponentType>
[[vk::ext_instruction(/* OpMatrixTimesScalar */ 143)]]
ResultType MatrixTimesScalar(ResultType a, ComponentType b);

}  // namespace spv

#endif  // VULKAN_HLSL_COMMON_H_
