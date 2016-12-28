# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
set(LLVM_TARGET_DEFINITIONS Intrinsics.td)

tablegen(LLVM Intrinsics.gen -gen-intrinsic)

add_public_tablegen_target(intrinsics_gen)
