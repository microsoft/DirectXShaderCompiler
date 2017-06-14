///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcutil.h                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper class for dxcompiler.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once


#include "dxc/dxcapi.h"
#include "dxc/Support/microcom.h"
#include <memory>

namespace clang {
class DiagnosticsEngine;
}

namespace llvm {
class Module;
class raw_string_ostream;
}

namespace hlsl {
enum class SerializeDxilFlags : uint32_t;
class AbstractMemoryStream;
}

namespace dxcutil {
HRESULT ValidateAndAssembleToContainer(
    std::unique_ptr<llvm::Module> pM, CComPtr<IDxcBlob> &pOutputContainerBlob,
    CComPtr<IMalloc> &pMalloc, hlsl::SerializeDxilFlags SerializeFlags,
    CComPtr<hlsl::AbstractMemoryStream> &pModuleBitcode, bool bDebugInfo,
    clang::DiagnosticsEngine &Diag);
void GetValidatorVersion(unsigned *pMajor, unsigned *pMinor);
void AssembleToContainer(std::unique_ptr<llvm::Module> pM,
                         CComPtr<IDxcBlob> &pOutputContainerBlob,
                         CComPtr<IMalloc> &pMalloc,
                         hlsl::SerializeDxilFlags SerializeFlags,
                         CComPtr<hlsl::AbstractMemoryStream> &pModuleBitcode);
HRESULT Disassemble(IDxcBlob *pProgram, llvm::raw_string_ostream &Stream);
} // namespace dxcutil