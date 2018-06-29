///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcutil.h                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper code for dxcompiler.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/dxcapi.h"
#include "dxc/Support/microcom.h"
#include <memory>

#define DISABLE_GET_CUSTOM_DIAG_ID 1

namespace clang {
class DiagnosticsEngine;
}

namespace llvm {
class LLVMContext;
class MemoryBuffer;
class Module;
class raw_string_ostream;
class StringRef;
class Twine;
} // namespace llvm

namespace hlsl {
enum class SerializeDxilFlags : uint32_t;
class AbstractMemoryStream;
namespace options {
class MainArgs;
class DxcOpts;
} // namespace options
} // namespace hlsl

namespace dxcutil {
HRESULT ValidateAndAssembleToContainer(
    std::unique_ptr<llvm::Module> pM, CComPtr<IDxcBlob> &pOutputContainerBlob,
    IMalloc *pMalloc, hlsl::SerializeDxilFlags SerializeFlags,
    CComPtr<hlsl::AbstractMemoryStream> &pModuleBitcode, bool bDebugInfo
#if  !DISABLE_GET_CUSTOM_DIAG_ID
  , clang::DiagnosticsEngine &Diag
#endif
  );
void GetValidatorVersion(unsigned *pMajor, unsigned *pMinor);
void AssembleToContainer(std::unique_ptr<llvm::Module> pM,
                         CComPtr<IDxcBlob> &pOutputContainerBlob,
                         IMalloc *pMalloc,
                         hlsl::SerializeDxilFlags SerializeFlags,
                         CComPtr<hlsl::AbstractMemoryStream> &pModuleBitcode);
HRESULT Disassemble(IDxcBlob *pProgram, llvm::raw_string_ostream &Stream);
void ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                         hlsl::options::DxcOpts &opts,
                         hlsl::AbstractMemoryStream *pOutputStream,
                         _COM_Outptr_ IDxcOperationResult **ppResult,
                         bool &finished);
void CreateOperationResultFromOutputs(
    IDxcBlob *pResultBlob, CComPtr<IStream> &pErrorStream,
    const std::string &warnings, bool hasErrorOccurred,
    _COM_Outptr_ IDxcOperationResult **ppResult);

bool IsAbsoluteOrCurDirRelative(const llvm::Twine &T);

} // namespace dxcutil