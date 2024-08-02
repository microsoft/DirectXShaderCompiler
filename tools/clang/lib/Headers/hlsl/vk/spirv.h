// Copyright (c) 2024 Google LLC
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef _HLSL_VK_SPIRV_H_
#define _HLSL_VK_SPIRV_H_

namespace vk {

enum StorageClass {
  StorageClassWorkgroup = 4,
};

// An opaque type to represent a Spir-V pointer to the workgroup storage class.
// clang-format off
template <typename PointeeType>
using WorkgroupSpirvPointer = const vk::SpirvOpaqueType<
    /* OpTypePointer */ 32,
    vk::Literal<vk::integral_constant<uint, StorageClassWorkgroup> >,
    PointeeType>;
// clang-format on

// Returns an opaque Spir-V pointer to v. The memory object v's storage class
// modifier must be groupshared. If the incorrect storage class is used, then
// there will be a validation error, and it will not show the correct
template <typename T>
[[vk::ext_instruction(/* OpCopyObject */ 83)]] WorkgroupSpirvPointer<T>
GetGroupSharedAddress([[vk::ext_reference]] T v);

} // namespace vk

#endif // _HLSL_VK_SPIRV_H_
