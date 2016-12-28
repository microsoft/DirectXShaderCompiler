///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLMatrixLowerPass.h                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides a high level matrix lower pass.                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class ModulePass;
class PassRegistry;

/// \brief Create and return a pass that lower high level matrix.
/// Note that this pass is designed for use with the legacy pass manager.
ModulePass *createHLMatrixLowerPass();
void initializeHLMatrixLowerPassPass(llvm::PassRegistry&);

}
