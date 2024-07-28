///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcHashingContainerBuilder.cpp                                            //
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

HRESULT CreateDxcValidator(REFIID RIID, LPVOID *V);

class DxcHashingContainerBuilder : public DxcContainerBuilder {
public:
  DxcHashingContainerBuilder(IMalloc *Malloc) : DxcContainerBuilder(Malloc) {}

  DXC_MICROCOM_TM_ALLOC(DxcHashingContainerBuilder);

  HRESULT STDMETHODCALLTYPE Load(IDxcBlob *DxilContainerHeader)
      override; // Loads DxilContainer to the builder
  HRESULT STDMETHODCALLTYPE SerializeContainer(IDxcOperationResult **Result)
      override; // Builds a container of the given container builder state

private:
  // Function to compute hash when valid dxil container is built
  // This is nullptr if loaded container has invalid hash
  HASH_FUNCTION_PROTO *m_HashFunction;

  void FindHashFunctionFromSource(const DxilContainerHeader *ContainerHeader);
  void HashAndUpdate(DxilContainerHeader *ContainerHeader);
};

HRESULT STDMETHODCALLTYPE DxcHashingContainerBuilder::Load(IDxcBlob *Source) {
  HRESULT HR = DxcContainerBuilder::Load(Source);
  if (SUCCEEDED(HR)) {
    const DxilContainerHeader *Header =
        (DxilContainerHeader *)Source->GetBufferPointer();
    FindHashFunctionFromSource(Header);
  }
  return HR;
}

HRESULT STDMETHODCALLTYPE
DxcHashingContainerBuilder::SerializeContainer(IDxcOperationResult **Result) {
  HRESULT HR = DxcContainerBuilder::SerializeContainer(Result);
  if (!SUCCEEDED(HR))
    return HR;

  if (Result != nullptr && *Result != nullptr) {
    HRESULT HR;
    (*Result)->GetStatus(&HR);
    if (SUCCEEDED(HR)) {
      CComPtr<IDxcBlob> pObject;
      HR = (*Result)->GetResult(&pObject);
      if (SUCCEEDED(HR)) {
        LPVOID PTR = pObject->GetBufferPointer();
        if (IsDxilContainerLike(PTR, pObject->GetBufferSize()))
          HashAndUpdate((DxilContainerHeader *)PTR);
      }
    }
  }
  return HR;
}

void DxcHashingContainerBuilder::FindHashFunctionFromSource(
    const DxilContainerHeader *ContainerHeader) {
  DXASSERT(ContainerHeader != nullptr &&
               IsDxilContainerLike(ContainerHeader,
                                   ContainerHeader->ContainerSizeInBytes),
           "otherwise load function should have returned an error.");
  static const uint32_t HashStartOffset =
      offsetof(struct DxilContainerHeader, Version);
  const BYTE *DataToHash = (const BYTE *)ContainerHeader + HashStartOffset;
  UINT AmountToHash = ContainerHeader->ContainerSizeInBytes - HashStartOffset;
  BYTE Result[DxilContainerHashSize];
  ComputeHashRetail(DataToHash, AmountToHash, Result);
  if (0 == memcmp(Result, ContainerHeader->Hash.Digest, sizeof(Result))) {
    m_HashFunction = ComputeHashRetail;
  } else {
    ComputeHashDebug(DataToHash, AmountToHash, Result);
    if (0 == memcmp(Result, ContainerHeader->Hash.Digest, sizeof(Result)))
      m_HashFunction = ComputeHashDebug;
    else
      m_HashFunction = nullptr;
  }
}

// For Internal hash function.
void DxcHashingContainerBuilder::HashAndUpdate(
    DxilContainerHeader *ContainerHeader) {
  if (m_HashFunction != nullptr) {
    DXASSERT(ContainerHeader != nullptr,
             "Otherwise serialization should have failed.");
    static const UINT32 HashStartOffset =
        offsetof(struct DxilContainerHeader, Version);
    const BYTE *DataToHash = (const BYTE *)ContainerHeader + HashStartOffset;
    UINT AmountToHash = ContainerHeader->ContainerSizeInBytes - HashStartOffset;
    m_HashFunction(DataToHash, AmountToHash, ContainerHeader->Hash.Digest);
  }
}

HRESULT CreateDxcHashingContainerBuilder(REFIID RRID, LPVOID *V) {
  // Call dxil.dll's containerbuilder
  *V = nullptr;
  CComPtr<DxcHashingContainerBuilder> Result(
      DxcHashingContainerBuilder::Alloc(DxcGetThreadMallocNoRef()));
  if (nullptr == Result.p)
    return E_OUTOFMEMORY;

  Result->Init();
  return Result->QueryInterface(RRID, V);
}