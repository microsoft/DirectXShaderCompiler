///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLOperationLower.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Lower functions to lower HL operations to DXIL operations.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class Instruction;
class Value;
class Function;
}

namespace hlsl {
class HLModule;
class DxilResourceBase;

void TranslateBuiltinOperations(
    HLModule &HLM,
    std::unordered_map<llvm::Instruction *, llvm::Value *> &handleMap);
}