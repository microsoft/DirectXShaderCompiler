///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// InitializePasses.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/Global.h"

namespace llvm {
class PassRegistry;
}

void __cdecl initializeDxilConvPasses(llvm::PassRegistry &Registry);

namespace hlsl {
HRESULT SetupRegistryPassForDxilConvPasses();
}
