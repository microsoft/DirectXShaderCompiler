///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// assert.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "assert.h"
#include "windows.h"

void llvm_assert(_In_z_ const char *_Message,
                 _In_z_ const char *_File,
                 _In_ unsigned _Line) {
  RaiseException(STATUS_LLVM_ASSERT, 0, 0, 0);
}
