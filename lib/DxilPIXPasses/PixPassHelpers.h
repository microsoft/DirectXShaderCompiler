///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.h  														 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace PIXPassHelpers
{
	bool IsAllocateRayQueryInstruction(llvm::Value* Val);
    llvm::CallInst* CreateUAV(hlsl::DxilModule& DM, llvm::IRBuilder<>& Builder,
                                  unsigned int registerId, const char *name);
    llvm::CallInst* CreateHandleForResource(hlsl::DxilModule& DM, llvm::IRBuilder<>& Builder,
        hlsl::DxilResourceBase * resource,
        const char* name);
}