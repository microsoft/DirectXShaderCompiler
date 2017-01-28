///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLOperationLower.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
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
class HLSLExtensionsCodegenHelper;

void TranslateBuiltinOperations(
    HLModule &HLM,
    std::unordered_map<llvm::Instruction *, llvm::Value *> &handleMap,
    HLSLExtensionsCodegenHelper *extCodegenHelper);
}