///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxr.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxr console program.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/microcom.h"
#include <vector>
#include <string>

#include "dxc/dxcapi.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"

inline bool wcsieq(LPCWSTR a, LPCWSTR b) { return _wcsicmp(a, b) == 0; }

using namespace dxc;

class DxrContext {

private:
  bool m_outputWarnings;
  LPCWSTR m_pEntryPoint;
  LPCWSTR m_pName;
  DxcDefine *m_pDefines;
  UINT32 m_definesCount;
  DxcDllSupport& m_dxcSupport;

public:
  DxrContext(LPCWSTR pName, LPCWSTR pEntryPoint, DxcDefine *pDefines,
             UINT32 definesCount, bool outputWarnings,
             DxcDllSupport& dxcSupport) :
    m_pName(pName), m_pEntryPoint(pEntryPoint), m_pDefines(pDefines),
    m_definesCount(definesCount), m_outputWarnings(outputWarnings),
    m_dxcSupport(dxcSupport) {
  }

  void RunRemoveUnusedGlobals();
  void RunRewriteUnchanged();
  HRESULT ReadFromFile(LPCWSTR pFileName, _COM_Outptr_ IDxcBlobEncoding** pBlobEncoding);
};

void DxrContext::RunRemoveUnusedGlobals() {
  CComPtr<IDxcRewriter> pRewriter;
  CComPtr<IDxcOperationResult> pRewriteResult;
  CComPtr<IDxcBlobEncoding> pBlobEncoding;

  IFT_Data(ReadFromFile(m_pName, &pBlobEncoding), m_pName);
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcRewriter, &pRewriter));
  IFT(pRewriter->RemoveUnusedGlobals(pBlobEncoding, m_pEntryPoint, m_pDefines, m_definesCount, &pRewriteResult));

  WriteOperationResultToConsole(pRewriteResult, m_outputWarnings);
}

void DxrContext::RunRewriteUnchanged() {
  CComPtr<IDxcRewriter> pRewriter;
  CComPtr<IDxcOperationResult> pRewriteResult;
  CComPtr<IDxcBlobEncoding> pBlobEncoding;

  IFT(ReadFromFile(m_pName, &pBlobEncoding));
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcRewriter, &pRewriter));
  IFT(pRewriter->RewriteUnchanged(pBlobEncoding, m_pDefines, m_definesCount, &pRewriteResult));

  WriteOperationResultToConsole(pRewriteResult, m_outputWarnings);
}

class FileMapDxcBlobEncoding : public IDxcBlobEncoding {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CHandle m_FileHandle;
  CHandle m_MappingHandle;
  void* m_MappedView;
  UINT32 m_FileSize;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  FileMapDxcBlobEncoding() : m_dwRef(0), m_MappedView(nullptr) {
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcBlob, IDxcBlobEncoding>(this, iid, ppvObject);
  }

  ~FileMapDxcBlobEncoding() {
    if (m_MappedView != nullptr) {
      UnmapViewOfFile(m_MappedView);
    }
  }

  static HRESULT CreateForFile(_In_ LPCWSTR pFileName,
                               _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) {
    *ppBlobEncoding = nullptr;

    CComPtr<FileMapDxcBlobEncoding> pResult = new FileMapDxcBlobEncoding();
    HRESULT hr = pResult->Open(pFileName);
    if (FAILED(hr)) {
      return hr;
    }
    return pResult.QueryInterface(ppBlobEncoding);
  }

  virtual LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override {
    return m_MappedView;
  }
  virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override {
    return m_FileSize;
  }
  virtual HRESULT STDMETHODCALLTYPE GetEncoding(_Out_ BOOL *pKnown, _Out_ UINT32 *pCodePage) {
    *pKnown = FALSE;
    *pCodePage = 0;
    return S_OK;
  }

  HRESULT Open(_In_ LPCWSTR pFileName) {
    DXASSERT_NOMSG(m_FileHandle == nullptr);

    HANDLE fileHandle = CreateFileW(pFileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (fileHandle == INVALID_HANDLE_VALUE) {
      return HRESULT_FROM_WIN32(GetLastError());
    }
    m_FileHandle.Attach(fileHandle);

    HANDLE mappingHandle = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mappingHandle == INVALID_HANDLE_VALUE) {
      return HRESULT_FROM_WIN32(GetLastError());
    }
    m_MappingHandle.Attach(mappingHandle);

    void* fileView = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (fileView == nullptr) {
      return HRESULT_FROM_WIN32(GetLastError());
    }
    m_MappedView = fileView;

    LARGE_INTEGER FileSize;
    if (!GetFileSizeEx(fileHandle, &FileSize)) {
      return HRESULT_FROM_WIN32(GetLastError());
    }
    if (FileSize.u.HighPart != 0 || FileSize.u.LowPart == UINT_MAX) {
      return DXC_E_INPUT_FILE_TOO_LARGE;
    }
    m_FileSize = FileSize.u.LowPart;

    return S_OK;
  }
};

_Use_decl_annotations_
HRESULT DxrContext::ReadFromFile(LPCWSTR pFileName, IDxcBlobEncoding** pBlobEncoding) {
  return FileMapDxcBlobEncoding::CreateForFile(pFileName, pBlobEncoding);
}

void PrintUsage() {
  wprintf(L"Usage: dxr.exe MODE FILE OPTIONS\n");
  wprintf(L"MODE can be either -unchanged or -remove-unused-globals.\n");
  wprintf(L"FILE is the .hlsl file to be rewritten.\n");
  wprintf(L"  Note that this file will be read using the system default Windows ANSI code page.\n");
  wprintf(L"OPTIONS currently supports:\n"
          L"  -E<entry point>\n"
          L"  -D<define-name>\n"
          L"  -D<define-name>=<define-value>\n"
          L"  -external <dxcompiler-path> <entry-point>\n"
          L"  -no-warnings\n");
}

int __cdecl wmain(int argc, const wchar_t **argv_) {
  if (argc < 2 || wcsieq(argv_[1], L"-?") || wcsieq(argv_[1], L"/?")) {
    PrintUsage();
    return 0;
  }

  if (argc < 3) {
    PrintUsage();
    return 1;
  }

  // Determine type of rewrite
  LPCWSTR pModeName = argv_[1];
  int modeNum = 0;

  if (wcsieq(pModeName, L"-unchanged")) {
    modeNum = 0;
  }
  else if (wcsieq(pModeName, L"-remove-unused-globals")) {
    modeNum = 1;
  }
  else {
    printf("Mode does not exist: [%S].\n", pModeName);
    return 1;
  }

  // Get the file name
  LPCWSTR pFileName = argv_[2];

  // Parse command line options
  try {
    DxcDllSupport dxcSupport;
    bool outputWarnings = true;
    LPCWSTR pEntryPoint = nullptr;
    int definesCount = 0;
    std::vector<DxcDefine> definesVector;
    std::vector<std::wstring> definesPieces; // This ensures that the memory needed for the DXCDefine ptrs won't get freed too soon

    for (int i = 2; i < argc; i++) {
      std::wstring argString(argv_[i]);
      std::wstring start = argString.substr(0, 2);

      if (wcsieq(argv_[i], L"-external")) {
        if (dxcSupport.IsEnabled()) {
          printf("-external already specified\n");
          return 1;
        }
        if (i+2 >= argc) {
          printf("-external requires a DLL name and function entry point\n");
          return 1;
        }
        ++i;
        CW2A entryFnName(argv_[i+1]);
        HRESULT hrLoad = dxcSupport.InitializeForDll(argv_[i], entryFnName);
        if (FAILED(hrLoad)) {
          wprintf(L"Unable to load support for external DLL %s with function %s - 0x%08x\n", argv_[i], argv_[i+1], hrLoad);
          return 1;
        }
        ++i; // also consumed the function name
        continue;
      }

      if (wcsieq(argv_[i], L"-no-warnings")) {
        outputWarnings = false;
        continue;
      }

      if (wcsieq(start.c_str(), L"-E") || wcsieq(start.c_str(), L"/E")) {
         pEntryPoint = argv_[i];
         pEntryPoint += 2;
         continue;
      }

      if (wcsieq(start.c_str(), L"-D") || wcsieq(start.c_str(), L"/D")) {
        std::wstring thisDefine = argv_[i];
        int index = thisDefine.find(L"=");
        if (index != std::wstring::npos) {
          std::wstring tempName = thisDefine.substr(2, index - 2);
          std::wstring tempVal = thisDefine.substr(index + 1, std::wstring::npos);

          definesPieces.push_back(tempName);
          definesPieces.push_back(tempVal);
        }
        else {
          LPCWSTR name = argv_[i];
          name += 2;
          LPCWSTR value = nullptr;
          definesVector.push_back(GetDefine(name, value));
        }
        definesCount++;
      }
    }

    // Now that definesPieces will no longer be changed, use it to construct the DXCDefines
    for (size_t i = 0; i < definesPieces.size(); i += 2) {
      definesVector.push_back(GetDefine(definesPieces[i].c_str(), definesPieces[i + 1].c_str()));
    }

    DxcDefine *pDefinesArray = nullptr;
    if (!definesVector.empty())
      pDefinesArray = definesVector.data(); 

    EnsureEnabled(dxcSupport);
    DxrContext context(pFileName, pEntryPoint, pDefinesArray, definesCount, outputWarnings, dxcSupport);

    switch (modeNum) {
    case 0:
      context.RunRewriteUnchanged();
      break;
    case 1:
      if (pEntryPoint == nullptr) {
        printf("Cannot use -remove-unused-globals without specifying an entry point.\n");
        return 1;
      }
      context.RunRemoveUnusedGlobals();
      break;
    }
  }
  catch (const ::hlsl::Exception& hlslException) {
    try {
      const char* msg = hlslException.what();
      Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as UTF-8 because we use ASCII contents only
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer), "Compilation failed - error code 0x%08x.", hlslException.hr);
        msg = printBuffer;
      }

      std::string textMessage;
      bool lossy;
      if (!Unicode::UTF8ToConsoleString(msg, &textMessage, &lossy) || lossy) {
        // Do a direct assignment as a last-ditch effort and print out as UTF-8.
        textMessage = msg;
      }

      printf("%s\n", textMessage.c_str());
    }
    catch (...) {
      printf("Compilation failed - unable to retrieve error message.\n");
    }

    return 1;
  }
  catch (std::bad_alloc&) {
    printf("Compilation failed - out of memory.\n");
    return 1;
  }
  catch (...) {
    printf("Compilation failed - unable to retrieve error message.\n");
    return 1;
  }

  return 0;
}
