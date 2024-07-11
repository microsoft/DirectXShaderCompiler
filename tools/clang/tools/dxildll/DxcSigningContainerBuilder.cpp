///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcSigningContainerBuilder.cpp                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the Dxil Container Builder                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "DxilHash.h"
#include "dxc/DxilContainer/DxcContainerBuilder.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.h"

#include <algorithm>

using namespace hlsl;

HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

class DxcSigningContainerBuilder : public DxcContainerBuilder {
public:
  DxcSigningContainerBuilder(IMalloc *pMalloc) : DxcContainerBuilder(pMalloc) {}

  DXC_MICROCOM_TM_ALLOC(DxcSigningContainerBuilder);

  HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pDxilContainerHeader)
      override; // Loads DxilContainer to the builder
  HRESULT STDMETHODCALLTYPE
  SerializeContainer(_Out_ IDxcOperationResult **ppResult)
      override; // Builds a container of the given container builder state

private:
  // Function to compute hash when valid dxil container is built
  // This is nullptr if loaded container has invalid hash
  HASH_FUNCTION_PROTO *m_pHashFunction;

  void FindHashFunctionFromSource(const DxilContainerHeader *pContainerHeader);
  void HashAndUpdate(DxilContainerHeader *pContainerHeader);
};

HRESULT STDMETHODCALLTYPE
DxcSigningContainerBuilder::Load(_In_ IDxcBlob *pSource) {
  HRESULT hr = DxcContainerBuilder::Load(pSource);
  if (SUCCEEDED(hr)) {
    const DxilContainerHeader *pHeader =
        (DxilContainerHeader *)pSource->GetBufferPointer();
    FindHashFunctionFromSource(pHeader);
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE DxcSigningContainerBuilder::SerializeContainer(
    _Out_ IDxcOperationResult **ppResult) {
  HRESULT hr_saved = DxcContainerBuilder::SerializeContainer(ppResult);
  if (!SUCCEEDED(hr_saved)) {
    return hr_saved;
  }
  if (ppResult != nullptr && *ppResult != nullptr) {
    HRESULT hr;
    (*ppResult)->GetStatus(&hr);
    if (SUCCEEDED(hr)) {
      CComPtr<IDxcBlob> pObject;
      hr = (*ppResult)->GetResult(&pObject);
      if (SUCCEEDED(hr)) {
        LPVOID ptr = pObject->GetBufferPointer();
        if (IsDxilContainerLike(ptr, pObject->GetBufferSize())) {
          HashAndUpdate((DxilContainerHeader *)ptr);
        }
      }
    }
  }
  return hr_saved;
}

void DxcSigningContainerBuilder::FindHashFunctionFromSource(
    const DxilContainerHeader *pContainerHeader) {
  DXASSERT(pContainerHeader != nullptr &&
               IsDxilContainerLike(pContainerHeader,
                                   pContainerHeader->ContainerSizeInBytes),
           "otherwise load function should have returned an error.");
  static const UINT32 HashStartOffset =
      offsetof(struct DxilContainerHeader, Version);
  const BYTE *pDataToHash = (const BYTE *)pContainerHeader + HashStartOffset;
  UINT AmountToHash = pContainerHeader->ContainerSizeInBytes - HashStartOffset;
  BYTE result[DxilContainerHashSize];
  ComputeHashRetail(pDataToHash, AmountToHash, result);
  if (0 == memcmp(result, pContainerHeader->Hash.Digest, sizeof(result))) {
    m_pHashFunction = ComputeHashRetail;
  } else {
    ComputeHashDebug(pDataToHash, AmountToHash, result);
    if (0 == memcmp(result, pContainerHeader->Hash.Digest, sizeof(result))) {
      m_pHashFunction = ComputeHashDebug;
    } else {
      m_pHashFunction = nullptr;
    }
  }
}

// For Internal hash function.
void DxcSigningContainerBuilder::HashAndUpdate(
    DxilContainerHeader *pContainerHeader) {
  if (m_pHashFunction != nullptr) {
    DXASSERT(pContainerHeader != nullptr,
             "Otherwise serialization should have failed.");
    static const UINT32 HashStartOffset =
        offsetof(struct DxilContainerHeader, Version);
    const BYTE *pDataToHash = (const BYTE *)pContainerHeader + HashStartOffset;
    UINT AmountToHash =
        pContainerHeader->ContainerSizeInBytes - HashStartOffset;
    m_pHashFunction(pDataToHash, AmountToHash, pContainerHeader->Hash.Digest);
  }
}

HRESULT CreateDxcSigningContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  // Call dxil.dll's containerbuilder
  *ppv = nullptr;
  CComPtr<DxcSigningContainerBuilder> Result(
      DxcSigningContainerBuilder::Alloc(DxcGetThreadMallocNoRef()));
  IFROOM(Result.p);
  Result->Init();
  return Result->QueryInterface(riid, ppv);
}