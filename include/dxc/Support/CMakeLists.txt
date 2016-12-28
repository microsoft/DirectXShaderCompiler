# Copyright (C) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license. See COPYRIGHT in the project root for full license information.
# TableGen HLSL options.
set(LLVM_TARGET_DEFINITIONS HLSLOptions.td)
tablegen(LLVM HLSLOptions.inc -gen-opt-parser-defs)
add_public_tablegen_target(TablegenHLSLOptions)
