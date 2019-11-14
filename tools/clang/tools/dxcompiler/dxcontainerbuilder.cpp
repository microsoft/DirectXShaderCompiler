///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcontainerbuilder.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the Dxil Container Builder                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxillib.h"

#include <algorithm>
#include "llvm/ADT/SmallVector.h"

using namespace hlsl;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

class DxcContainerBuilder : public IDxcContainerBuilder {
public:
  HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pDxilContainerHeader) override; // Loads DxilContainer to the builder
  HRESULT STDMETHODCALLTYPE AddPart(_In_ UINT32 fourCC, _In_ IDxcBlob *pSource) override; // Add the given part with fourCC
  HRESULT STDMETHODCALLTYPE RemovePart(_In_ UINT32 fourCC) override;                // Remove the part with fourCC
  HRESULT STDMETHODCALLTYPE SerializeContainer(_Out_ IDxcOperationResult **ppResult) override; // Builds a container of the given container builder state

  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcContainerBuilder)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcContainerBuilder>(this, riid, ppvObject);
  }

  void Init(const char *warning) {
    m_warning = warning;
    m_RequireValidation = false;
  }

private:
  DXC_MICROCOM_TM_REF_FIELDS()

  class DxilPart {
  public:
    UINT32 m_fourCC;
    CComPtr<IDxcBlob> m_Blob;
    DxilPart(UINT32 fourCC, IDxcBlob *pSource) : m_fourCC(fourCC), m_Blob(pSource) {}
  };
  typedef llvm::SmallVector<DxilPart, 8> PartList;

  PartList m_parts;
  CComPtr<IDxcBlob> m_pContainer; 
  const char *m_warning;
  bool m_RequireValidation;

  UINT32 ComputeContainerSize();
  HRESULT UpdateContainerHeader(AbstractMemoryStream *pStream, uint32_t containerSize);
  HRESULT UpdateOffsetTable(AbstractMemoryStream *pStream);
  HRESULT UpdateParts(AbstractMemoryStream *pStream);
};

HRESULT STDMETHODCALLTYPE DxcContainerBuilder::Load(_In_ IDxcBlob *pSource) {
  DxcThreadMalloc TM(m_pMalloc);
  try {
    IFTBOOL(m_pContainer == nullptr && pSource != nullptr &&
      IsDxilContainerLike(pSource->GetBufferPointer(),
        pSource->GetBufferSize()),
      E_INVALIDARG);
    m_pContainer = pSource;
    const DxilContainerHeader *pHeader = (DxilContainerHeader *)pSource->GetBufferPointer();
    for (DxilPartIterator it = begin(pHeader), itEnd = end(pHeader); it != itEnd; ++it) {
      const DxilPartHeader *pPartHeader = *it;
      CComPtr<IDxcBlobEncoding> pBlob;
      IFT(DxcCreateBlobWithEncodingFromPinned((const void *)(pPartHeader + 1), pPartHeader->PartSize, CP_UTF8, &pBlob));
      PartList::iterator itPartList = std::find_if(m_parts.begin(), m_parts.end(), [&](DxilPart part) {
        return part.m_fourCC == pPartHeader->PartFourCC;
      });
      IFTBOOL(itPartList == m_parts.end(), DXC_E_DUPLICATE_PART);
      m_parts.emplace_back(DxilPart(pPartHeader->PartFourCC, pBlob));
    }
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}


HRESULT STDMETHODCALLTYPE DxcContainerBuilder::AddPart(_In_ UINT32 fourCC, _In_ IDxcBlob *pSource) {
  DxcThreadMalloc TM(m_pMalloc);
  try {
    IFTBOOL(pSource != nullptr && !IsDxilContainerLike(pSource->GetBufferPointer(),
      pSource->GetBufferSize()),
      E_INVALIDARG);
    // Only allow adding private data, debug info name and root signature for now
    IFTBOOL(
        fourCC == DxilFourCC::DFCC_RootSignature || 
        fourCC == DxilFourCC::DFCC_ShaderDebugName ||
        fourCC == DxilFourCC::DFCC_PrivateData, 
      E_INVALIDARG);
    PartList::iterator it = std::find_if(m_parts.begin(), m_parts.end(), [&](DxilPart part) {
      return part.m_fourCC == fourCC;
    });
    IFTBOOL(it == m_parts.end(), DXC_E_DUPLICATE_PART);
    m_parts.emplace_back(DxilPart(fourCC, pSource));
    if (fourCC == DxilFourCC::DFCC_RootSignature) {
      m_RequireValidation = true;
    }
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

HRESULT STDMETHODCALLTYPE DxcContainerBuilder::RemovePart(_In_ UINT32 fourCC) {
  DxcThreadMalloc TM(m_pMalloc);
  try {
    IFTBOOL(fourCC == DxilFourCC::DFCC_ShaderDebugInfoDXIL ||
                fourCC == DxilFourCC::DFCC_ShaderDebugName ||
                fourCC == DxilFourCC::DFCC_RootSignature ||
                fourCC == DxilFourCC::DFCC_PrivateData ||
                fourCC == DxilFourCC::DFCC_ShaderStatistics,
            E_INVALIDARG); // You can only remove debug info, debug info name, rootsignature, or private data blob
    PartList::iterator it =
      std::find_if(m_parts.begin(), m_parts.end(),
        [&](DxilPart part) { return part.m_fourCC == fourCC; });
    IFTBOOL(it != m_parts.end(), DXC_E_MISSING_PART);
    m_parts.erase(it);
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

HRESULT STDMETHODCALLTYPE DxcContainerBuilder::SerializeContainer(_Out_ IDxcOperationResult **ppResult) {
  DxcThreadMalloc TM(m_pMalloc);
  try {
    // Allocate memory for new dxil container.
    uint32_t ContainerSize = ComputeContainerSize();
    CComPtr<AbstractMemoryStream> pMemoryStream;
    CComPtr<IDxcBlob> pResult;
    IFT(CreateMemoryStream(m_pMalloc, &pMemoryStream));
    IFT(pMemoryStream->QueryInterface(&pResult));
    IFT(pMemoryStream->Reserve(ContainerSize))
    
    // Update Dxil Container
    IFT(UpdateContainerHeader(pMemoryStream, ContainerSize));

    // Update offset Table
    IFT(UpdateOffsetTable(pMemoryStream));
    
    // Update Parts
    IFT(UpdateParts(pMemoryStream));

    CComPtr<IDxcBlobUtf8> pValErrorUtf8;
    HRESULT valHR = S_OK;
    if (m_RequireValidation) {
      CComPtr<IDxcValidator> pValidator;
      IFT(CreateDxcValidator(IID_PPV_ARGS(&pValidator)));
      CComPtr<IDxcOperationResult> pValidationResult;
      IFT(pValidator->Validate(pResult, DxcValidatorFlags_RootSignatureOnly, &pValidationResult));
      IFT(pValidationResult->GetStatus(&valHR));
      if (FAILED(valHR)) {
        CComPtr<IDxcBlobEncoding> pValError;
        IFT(pValidationResult->GetErrorBuffer(&pValError));
        if (pValError->GetBufferPointer() && pValError->GetBufferSize())
          IFT(hlsl::DxcGetBlobAsUtf8(pValError, m_pMalloc, &pValErrorUtf8));
      }
    }
    // Combine existing warnings and errors from validation
    CComPtr<IDxcBlobEncoding> pErrorBlob;
    CDxcMallocHeapPtr<char> errorHeap(m_pMalloc);
    SIZE_T warningLength = m_warning ? strlen(m_warning) : 0;
    SIZE_T valErrorLength = pValErrorUtf8 ? pValErrorUtf8->GetStringLength() : 0;
    SIZE_T totalErrorLength = warningLength + valErrorLength;
    if (totalErrorLength) {
      SIZE_T errorSizeInBytes = totalErrorLength + 1;
      errorHeap.AllocateBytes(errorSizeInBytes);
      if (warningLength)
        memcpy(errorHeap.m_pData, m_warning, warningLength);
      if (valErrorLength)
        memcpy(errorHeap.m_pData + warningLength,
               pValErrorUtf8->GetStringPointer(),
               valErrorLength);
      errorHeap.m_pData[totalErrorLength] = L'\0';
      IFT(hlsl::DxcCreateBlobWithEncodingOnMalloc(
        errorHeap.m_pData, m_pMalloc, errorSizeInBytes,
        DXC_CP_UTF8, &pErrorBlob));
      errorHeap.Detach();
    }

    IFT(DxcResult::Create(valHR, DXC_OUT_OBJECT, {
        DxcOutputObject::DataOutput(DXC_OUT_OBJECT, pResult, DxcOutNoName),
        DxcOutputObject::DataOutput(DXC_OUT_ERRORS, pErrorBlob, DxcOutNoName)
      }, ppResult));
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
}

UINT32 DxcContainerBuilder::ComputeContainerSize() {
  UINT32 partsSize = 0;
  for (DxilPart part : m_parts) {
    partsSize += part.m_Blob->GetBufferSize();
  }
  return GetDxilContainerSizeFromParts(m_parts.size(), partsSize);
}

HRESULT DxcContainerBuilder::UpdateContainerHeader(AbstractMemoryStream *pStream, uint32_t containerSize) {
  DxilContainerHeader header;
  InitDxilContainer(&header, m_parts.size(), containerSize);
  ULONG cbWritten;
  IFR(pStream->Write(&header, sizeof(DxilContainerHeader), &cbWritten));
  if (cbWritten != sizeof(DxilContainerHeader)) {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT DxcContainerBuilder::UpdateOffsetTable(AbstractMemoryStream *pStream) {
  UINT32 offset = sizeof(DxilContainerHeader) + GetOffsetTableSize(m_parts.size());
  for (size_t i = 0; i < m_parts.size(); ++i) {
    ULONG cbWritten;
    IFR(pStream->Write(&offset, sizeof(UINT32), &cbWritten));
    if (cbWritten != sizeof(UINT32)) { return E_FAIL; }
    offset += sizeof(DxilPartHeader) + m_parts[i].m_Blob->GetBufferSize();
  }
  return S_OK;
}

HRESULT DxcContainerBuilder::UpdateParts(AbstractMemoryStream *pStream) {
  for (size_t i = 0; i < m_parts.size(); ++i) {
    ULONG cbWritten;
    CComPtr<IDxcBlob> pBlob = m_parts[i].m_Blob;
    // Write part header
    DxilPartHeader partHeader = { m_parts[i].m_fourCC, (uint32_t) pBlob->GetBufferSize() };
    IFR(pStream->Write(&partHeader, sizeof(DxilPartHeader), &cbWritten));
    if (cbWritten != sizeof(DxilPartHeader)) { return E_FAIL; }
    // Write part content
    IFR(pStream->Write(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &cbWritten));
    if (cbWritten != pBlob->GetBufferSize()) { return E_FAIL; }
  }
  return S_OK;
}

HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  // Call dxil.dll's containerbuilder 
  *ppv = nullptr;
  const char *warning;
  HRESULT hr = DxilLibCreateInstance(CLSID_DxcContainerBuilder, (IDxcContainerBuilder**)ppv);
  if (FAILED(hr)) {
    warning = "Unable to create container builder from dxil.dll. Resulting container will not be signed.\n";
  }
  else {
    return hr;
  }

  CComPtr<DxcContainerBuilder> Result = DxcContainerBuilder::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(Result.p);
  Result->Init(warning);
  return Result->QueryInterface(riid, ppv);
}
