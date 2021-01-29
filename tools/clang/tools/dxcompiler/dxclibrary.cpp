///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxclibrary.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler Library helper.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"

#include "dxc/dxcapi.internal.h"
#include "dxc/dxctools.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DXIL/DxilPDB.h"

#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace hlsl;

#ifdef _WIN32
// Temporary: Define these here until a better header location is found.
namespace hlsl {
HRESULT CreateDxilShaderReflection(const DxilPartHeader *pModulePart, const DxilPartHeader *pRDATPart, REFIID iid, void **ppvObject);
HRESULT CreateDxilLibraryReflection(const DxilPartHeader *pModulePart, const DxilPartHeader *pRDATPart, REFIID iid, void **ppvObject);
}
#endif

class DxcIncludeHandlerForFS : public IDxcIncludeHandler {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcIncludeHandlerForFS)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                                   // Candidate filename.
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource  // Resultant source object for included file, nullptr if not found.
    ) override {
    try {
      CComPtr<IDxcBlobEncoding> pEncoding;
      HRESULT hr = ::hlsl::DxcCreateBlobFromFile(m_pMalloc, pFilename, nullptr, &pEncoding);
      if (SUCCEEDED(hr)) {
        *ppIncludeSource = pEncoding.Detach();
      }
      return hr;
    }
    CATCH_CPP_RETURN_HRESULT();
  }
};

class DxcCompilerArgs : public IDxcCompilerArgs {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  std::unordered_set<std::wstring> m_Strings;
  std::vector<LPCWSTR> m_Arguments;

  LPCWSTR AddArgument(LPCWSTR pArg) {
    auto it = m_Strings.insert(pArg);
    LPCWSTR pInternalVersion = (it.first)->c_str();
    m_Arguments.push_back(pInternalVersion);
    return pInternalVersion;
  }

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcCompilerArgs)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcCompilerArgs>(this, iid, ppvObject);
  }

  // Pass GetArguments() and GetCount() to Compile
  LPCWSTR* STDMETHODCALLTYPE GetArguments() override {
    return m_Arguments.data();
  }
  UINT32 STDMETHODCALLTYPE GetCount() override {
    return static_cast<UINT32>(m_Arguments.size());
  }

  // Add additional arguments or defines here, if desired.
  HRESULT STDMETHODCALLTYPE AddArguments(
    _In_opt_count_(argCount) LPCWSTR *pArguments,       // Array of pointers to arguments to add
    _In_ UINT32 argCount                                // Number of arguments to add
  ) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      for (UINT32 i = 0; i < argCount; ++i) {
        AddArgument(pArguments[i]);
      }
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }
  HRESULT STDMETHODCALLTYPE AddArgumentsUTF8(
    _In_opt_count_(argCount)LPCSTR *pArguments,         // Array of pointers to UTF-8 arguments to add
    _In_ UINT32 argCount                                // Number of arguments to add
  ) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      for (UINT32 i = 0; i < argCount; ++i) {
        AddArgument(CA2W(pArguments[i]));
      }
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }
  HRESULT STDMETHODCALLTYPE AddDefines(
      _In_count_(defineCount) const DxcDefine *pDefines, // Array of defines
      _In_ UINT32 defineCount                            // Number of defines
  ) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      for (UINT32 i = 0; i < defineCount; ++i) {
        LPCWSTR Name = pDefines[i].Name;
        LPCWSTR Value = pDefines[i].Value;
        AddArgument(L"-D");
        if (!Value) {
          // L"-D", L"<name>"
          AddArgument(Name);
          continue;
        }
        // L"-D", L"<name>=<value>"
        std::wstring defineArg;
        size_t size = 2 + wcslen(Name) + wcslen(Value);
        defineArg.reserve(size);
        defineArg = Name;
        defineArg += L"=";
        defineArg += pDefines[i].Value;
        AddArgument(defineArg.c_str());
      }
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  // This is used by BuildArguments to skip extra entry/profile arguments in the
  // arg list when already specified separatly.  This would lead to duplicate or
  // even contradictory arguments in the arg list, visible in debug information.
  HRESULT AddArgumentsOptionallySkippingEntryAndTarget(
      _In_opt_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments to add
      _In_ UINT32 argCount,                             // Number of arguments to add
      bool skipEntry, bool skipTarget) {
    DxcThreadMalloc TM(m_pMalloc);
    bool skipNext = false;
    for (UINT32 i = 0; i < argCount; ++i) {
      if (skipNext) {
        skipNext = false;
        continue;
      }
      if (skipEntry || skipTarget) {
        LPCWSTR arg = pArguments[i];
        UINT size = wcslen(arg);
        if (size >= 2) {
          if (arg[0] == L'-' || arg[0] == L'/') {
            if ((skipEntry && arg[1] == L'E') ||
                (skipTarget && arg[1] == L'T')) {
              skipNext = size == 2;  // skip next if value not joined
              continue;
            }
          }
        }
      }
      AddArgument(pArguments[i]);
    }
    return S_OK;
  }
};

class DxcUtils;

// DxcLibrary just wraps DxcUtils now.
class DxcLibrary : public IDxcLibrary {
private:
  DxcUtils &self;

public:
  DxcLibrary(DxcUtils &impl) : self(impl) {}
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;
  HRESULT STDMETHODCALLTYPE SetMalloc(_In_opt_ IMalloc *pMalloc) override;
  HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) override;
  HRESULT STDMETHODCALLTYPE CreateBlobFromFile(
    LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingFromPinned(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnHeapCopy(
      _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
      _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnMalloc(
    _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
  HRESULT STDMETHODCALLTYPE CreateIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) override;
  HRESULT STDMETHODCALLTYPE CreateStreamFromBlobReadOnly(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) override;
  HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
  HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override;
};

class DxcUtils : public IDxcUtils {
  friend class DxcLibrary;
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  DxcLibrary m_Library;
public:
  DxcUtils(IMalloc *pMalloc) : m_dwRef(0), m_pMalloc(pMalloc), m_Library(*this) {}
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcUtils)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    HRESULT hr = DoBasicQueryInterface<IDxcUtils>(this, iid, ppvObject);
    if (FAILED(hr)) {
      return DoBasicQueryInterface<IDxcLibrary>(&m_Library, iid, ppvObject);
    }
    return hr;
  }

  HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcCreateBlobFromBlob(pBlob, offset, length, ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE CreateBlobFromPinned(
    _In_bytecount_(size) LPCVOID pData, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcCreateBlobWithEncodingFromPinned(pData, size, codePage, pBlobEncoding);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  virtual HRESULT STDMETHODCALLTYPE MoveToBlob(
    _In_bytecount_(size) LPCVOID pData, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcCreateBlobWithEncodingOnMalloc(pData, pIMalloc, size, codePage, pBlobEncoding);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  virtual HRESULT STDMETHODCALLTYPE CreateBlob(
    _In_bytecount_(size) LPCVOID pData, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcCreateBlobWithEncodingOnHeapCopy(pData, size, codePage, pBlobEncoding);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  virtual HRESULT STDMETHODCALLTYPE LoadFile(
    _In_z_ LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcCreateBlobFromFile(pFileName, pCodePage, pBlobEncoding);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE CreateReadOnlyStreamFromBlob(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::CreateReadOnlyBlobStream(pBlob, ppStream);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  virtual HRESULT STDMETHODCALLTYPE CreateDefaultIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) override {
    DxcThreadMalloc TM(m_pMalloc);
    CComPtr<DxcIncludeHandlerForFS> result;
    result = DxcIncludeHandlerForFS::Alloc(m_pMalloc);
    if (result.p == nullptr) {
      return E_OUTOFMEMORY;
    }
    *ppResult = result.Detach();
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobUtf8 **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcGetBlobAsUtf8(pBlob, m_pMalloc, pBlobEncoding);
  }
  virtual HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobUtf16 **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    try {
      return ::hlsl::DxcGetBlobAsUtf16(pBlob, m_pMalloc, pBlobEncoding);
    }
    CATCH_CPP_RETURN_HRESULT();
  }


  virtual HRESULT STDMETHODCALLTYPE GetDxilContainerPart(    _In_ const DxcBuffer *pShader,
    _In_ UINT32 DxcPart,
    _Outptr_result_nullonfailure_ void **ppPartData,
    _Out_ UINT32 *pPartSizeInBytes) override {
    if (!ppPartData || !pPartSizeInBytes)
      return E_INVALIDARG;

    const DxilContainerHeader *pHeader = IsDxilContainerLike(pShader->Ptr, pShader->Size);
    if (!pHeader)
      return DXC_E_CONTAINER_INVALID;
    if (!IsValidDxilContainer(pHeader, pShader->Size))
      return DXC_E_CONTAINER_INVALID;
    DxilPartIterator it = std::find_if(begin(pHeader), end(pHeader), DxilPartIsType(DxcPart));
    if (it == end(pHeader))
      return DXC_E_MISSING_PART;

    *ppPartData = (void*)GetDxilPartData(*it);
    *pPartSizeInBytes = (*it)->PartSize;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateReflection(
    _In_ const DxcBuffer *pData, REFIID iid, void **ppvReflection) override {
#ifdef _WIN32
    if (!pData || !pData->Ptr || pData->Size < 8 || pData->Encoding != DXC_CP_ACP ||
        !ppvReflection)
      return E_INVALIDARG;

    DxcThreadMalloc TM(m_pMalloc);
    try {
      const DxilPartHeader *pModulePart = nullptr;
      const DxilPartHeader *pRDATPart = nullptr;

      // Is this a valid DxilContainer?
      if (const DxilContainerHeader *pHeader = IsDxilContainerLike(pData->Ptr, pData->Size)) {
        if (!IsValidDxilContainer(pHeader, pData->Size))
          return E_INVALIDARG;

        const DxilPartHeader *pDXILPart = nullptr;
        const DxilPartHeader *pDebugDXILPart = nullptr;
        const DxilPartHeader *pStatsPart = nullptr;
        for (DxilPartIterator it = begin(pHeader), E = end(pHeader); it != E; ++it) {
          const DxilPartHeader *pPart = *it;
          switch (pPart->PartFourCC) {
          case DFCC_DXIL:
            IFRBOOL(!pDXILPart, DXC_E_DUPLICATE_PART);  // Should only be one
            pDXILPart = pPart;
            break;
          case DFCC_ShaderDebugInfoDXIL:
            IFRBOOL(!pDebugDXILPart, DXC_E_DUPLICATE_PART);  // Should only be one
            pDebugDXILPart = pPart;
            break;
          case DFCC_ShaderStatistics:
            IFRBOOL(!pStatsPart, DXC_E_DUPLICATE_PART);  // Should only be one
            pStatsPart = pPart;
            break;
          case DFCC_RuntimeData:
            IFRBOOL(!pRDATPart, DXC_E_DUPLICATE_PART);  // Should only be one
            pRDATPart = pPart;
            break;
          }
        }

        // For now, pStatsPart contains module without function bodies for reflection.
        // If not found, fall back to DXIL part.
        pModulePart = pStatsPart ? pStatsPart : pDebugDXILPart ? pDebugDXILPart : pDXILPart;
        if (nullptr == pModulePart)
          return DXC_E_MISSING_PART;

      } else {
        // Not a container, try a statistics part that holds a valid program part.
        // In the future, this will just be the RDAT part.
        const DxilPartHeader *pPart = reinterpret_cast<const DxilPartHeader *>(pData->Ptr);
        if (pPart->PartSize < sizeof(DxilProgramHeader) ||
            pPart->PartSize + sizeof(DxilPartHeader) > pData->Size)
          return E_INVALIDARG;
        if (pPart->PartFourCC != DFCC_ShaderStatistics)
          return E_INVALIDARG;
        pModulePart = pPart;
        UINT32 SizeRemaining = pData->Size - (sizeof(DxilPartHeader) + pPart->PartSize);
        if (SizeRemaining > sizeof(DxilPartHeader)) {
          // Looks like we also have an RDAT part
          pPart = (DxilPartHeader*)(GetDxilPartData(pPart) + pPart->PartSize);
          if (pPart->PartSize < /*sizeof(RuntimeDataHeader)*/8 ||
              pPart->PartSize + sizeof(DxilPartHeader) > SizeRemaining)
            return E_INVALIDARG;
          if (pPart->PartFourCC != DFCC_RuntimeData)
            return E_INVALIDARG;
          pRDATPart = pPart;
        }
      }

      bool bIsLibrary = false;

      if (pModulePart) {
        if (pModulePart->PartFourCC != DFCC_DXIL &&
            pModulePart->PartFourCC != DFCC_ShaderDebugInfoDXIL &&
            pModulePart->PartFourCC != DFCC_ShaderStatistics) {
          return E_INVALIDARG;
        }
        const DxilProgramHeader *pProgramHeader =
          reinterpret_cast<const DxilProgramHeader*>(GetDxilPartData(pModulePart));
        if (!IsValidDxilProgramHeader(pProgramHeader, pModulePart->PartSize))
          return E_INVALIDARG;

        // If bitcode is too small, it's probably been stripped, and we cannot create reflection with it.
        if (pModulePart->PartSize - pProgramHeader->BitcodeHeader.BitcodeOffset < 4)
          return DXC_E_MISSING_PART;

        // Detect whether library, or if unrecognized program version.
        DXIL::ShaderKind SK = GetVersionShaderType(pProgramHeader->ProgramVersion);
        if (!(SK < DXIL::ShaderKind::Invalid))
          return E_INVALIDARG;
        bIsLibrary = DXIL::ShaderKind::Library == SK;
      }

      if (bIsLibrary) {
        IFR(hlsl::CreateDxilLibraryReflection(pModulePart, pRDATPart, iid, ppvReflection));
      } else {
        IFR(hlsl::CreateDxilShaderReflection(pModulePart, pRDATPart, iid, ppvReflection));
      }

      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
#else
    return E_NOTIMPL;
#endif
  }

  virtual HRESULT STDMETHODCALLTYPE BuildArguments(
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_z_ LPCWSTR pEntryPoint,               // Entry point name. (-E)
    _In_z_ LPCWSTR pTargetProfile,                // Shader profile to compile. (-T)
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 NumArguments,                     // Number of arguments
    _In_count_(NumDefines)
    const DxcDefine *pDefines,                    // Array of defines
    _In_ UINT32 NumDefines,                       // Number of defines
    _COM_Outptr_ IDxcCompilerArgs **ppArgs        // Arguments you can use with Compile() method
  ) override {
    DxcThreadMalloc TM(m_pMalloc);

    try {
      CComPtr<DxcCompilerArgs> pArgs = DxcCompilerArgs::Alloc(m_pMalloc);
      if (!pArgs)
        return E_OUTOFMEMORY;

      if (pSourceName) {
        IFR(pArgs->AddArguments(&pSourceName, 1));
      }
      if (pEntryPoint) {
        if (wcslen(pEntryPoint)) {
          LPCWSTR args[] = { L"-E", pEntryPoint };
          IFR(pArgs->AddArguments(args, _countof(args)));
        } else {
          pEntryPoint = nullptr;
        }
      }
      if (pTargetProfile) {
        if (wcslen(pTargetProfile)) {
          LPCWSTR args[] = { L"-T", pTargetProfile };
          IFR(pArgs->AddArguments(args, _countof(args)));
        } else {
          pTargetProfile = nullptr;
        }
      }
      if (pArguments && NumArguments) {
        IFR(pArgs->AddArgumentsOptionallySkippingEntryAndTarget(
          pArguments, NumArguments, !!pEntryPoint, !!pTargetProfile));
      }
      if (pDefines && NumDefines) {
        IFR(pArgs->AddDefines(pDefines, NumDefines));
      }

      *ppArgs = pArgs.Detach();
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  virtual HRESULT STDMETHODCALLTYPE
  GetPDBContents(_In_ IDxcBlob *pPDBBlob, _COM_Outptr_ IDxcBlob **ppHash,
                 _COM_Outptr_ IDxcBlob **ppContainer) override
  {
    DxcThreadMalloc TM(m_pMalloc);

    try {
      CComPtr<IStream> pStream;
      IFR(hlsl::CreateReadOnlyBlobStream(pPDBBlob, &pStream));
      IFR(hlsl::pdb::LoadDataFromStream(m_pMalloc, pStream, ppHash, ppContainer));
      return S_OK;
    }
    CATCH_CPP_RETURN_HRESULT();
  }

};

//////////////////////////////////////////////////////////////
// legacy DxcLibrary implementation that maps to DxcCompiler
ULONG STDMETHODCALLTYPE DxcLibrary::AddRef() {
  return self.AddRef();
}
ULONG STDMETHODCALLTYPE DxcLibrary::Release() {
  return self.Release();
}
HRESULT STDMETHODCALLTYPE DxcLibrary::QueryInterface(REFIID iid, void **ppvObject) {
  return self.QueryInterface(iid, ppvObject);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::SetMalloc(_In_opt_ IMalloc *pMalloc) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateBlobFromBlob(
  _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) {
  return self.CreateBlobFromBlob(pBlob, offset, length, ppResult);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateBlobFromFile(
  LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
  _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  return self.LoadFile(pFileName, pCodePage, pBlobEncoding);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateBlobWithEncodingFromPinned(
  _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
  _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  return self.CreateBlobFromPinned(pText, size, codePage, pBlobEncoding);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateBlobWithEncodingOnHeapCopy(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  return self.CreateBlob(pText, size, codePage, pBlobEncoding);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateBlobWithEncodingOnMalloc(
  _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
  _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  return self.MoveToBlob(pText, pIMalloc, size, codePage, pBlobEncoding);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateIncludeHandler(
  _COM_Outptr_ IDxcIncludeHandler **ppResult) {
  return self.CreateDefaultIncludeHandler(ppResult);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::CreateStreamFromBlobReadOnly(
  _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) {
  return self.CreateReadOnlyStreamFromBlob(pBlob, ppStream);
}

HRESULT STDMETHODCALLTYPE DxcLibrary::GetBlobAsUtf8(
  _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  CComPtr<IDxcBlobUtf8> pBlobUtf8;
  IFR(self.GetBlobAsUtf8(pBlob, &pBlobUtf8));
  IFR(pBlobUtf8->QueryInterface(pBlobEncoding));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DxcLibrary::GetBlobAsUtf16(
  _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
  CComPtr<IDxcBlobUtf16> pBlobUtf16;
  IFR(self.GetBlobAsUtf16(pBlob, &pBlobUtf16));
  IFR(pBlobUtf16->QueryInterface(pBlobEncoding));
  return S_OK;
}

HRESULT CreateDxcCompilerArgs(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcCompilerArgs> result = DxcCompilerArgs::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}

HRESULT CreateDxcUtils(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcUtils> result = DxcUtils::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
