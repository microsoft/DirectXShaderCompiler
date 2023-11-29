///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidator.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object.                                  //
// Merely adds the version queries to the Dxilvalidator                      //
// optionally with IDxcVersionInfo2 extensions                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/HLSL/DxcValidatorBase.h"
#include "dxc/HLSL/DxilValidation.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/MemoryBuffer.h"

#ifdef _WIN32
#include "dxcetw.h"
#endif

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

class DxcValidator : public DxcValidatorBase,
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
                     public IDxcVersionInfo2
#else
                     public IDxcVersionInfo
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcValidator)
  DxcValidator(IMalloc *pMalloc)
      : DxcValidatorBase(pMalloc), m_dwRef(0), m_pMalloc(pMalloc) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcValidator, IDxcValidator2,
                                 IDxcVersionInfo>(this, iid, ppvObject);
  }

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *pMajor, UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *pFlags) override;

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
  // IDxcVersionInfo2
  HRESULT STDMETHODCALLTYPE GetCommitInfo(UINT32 *pCommitCount,
                                          char **pCommitHash) override;
#endif
};

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(UINT32 *pMajor,
                                                   UINT32 *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  hlsl::GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
HRESULT STDMETHODCALLTYPE DxcValidator::GetCommitInfo(UINT32 *pCommitCount,
                                                      char **pCommitHash) {
  if (pCommitCount == nullptr || pCommitHash == nullptr)
    return E_INVALIDARG;

  char *const hash = (char *)CoTaskMemAlloc(
      8 + 1); // 8 is guaranteed by utils/GetCommitInfo.py
  if (hash == nullptr)
    return E_OUTOFMEMORY;
  std::strcpy(hash, clang::getGitCommitHash());

  *pCommitHash = hash;
  *pCommitCount = clang::getGitCommitCount();

  return S_OK;
}
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifndef NDEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  *pFlags |= DxcVersionInfoFlags_Internal;
  return S_OK;
}

HRESULT CreateDxcValidator(REFIID riid, LPVOID *ppv) {
  try {
    CComPtr<DxcValidator> result(
        DxcValidator::Alloc(DxcGetThreadMallocNoRef()));
    IFROOM(result.p);
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
