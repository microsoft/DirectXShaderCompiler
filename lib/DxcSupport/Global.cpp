///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Global.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //

//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"

#include <system_error>

#include "dxc/Support/WinIncludes.h"


void CheckLLVMErrorCode(const std::error_code &ec) {
  if (ec) {
    DXASSERT(ec.category() == std::system_category(), "unexpected LLVM exception code");
    throw hlsl::Exception(HRESULT_FROM_WIN32(ec.value()));
  }
}

static_assert(DXC_E_OVERLAPPING_SEMANTICS == 0x80AA0001, "Sanity check for DXC errors failed");
