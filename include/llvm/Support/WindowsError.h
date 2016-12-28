//===-- WindowsError.h - Support for mapping windows errors to posix-------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// WindowsError.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_SUPPORT_WINDOWSERROR_H
#define LLVM_SUPPORT_WINDOWSERROR_H

#include <system_error>

namespace llvm {
std::error_code mapWindowsError(unsigned EV);
}

#endif
