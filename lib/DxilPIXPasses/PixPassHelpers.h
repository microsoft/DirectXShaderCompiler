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
}