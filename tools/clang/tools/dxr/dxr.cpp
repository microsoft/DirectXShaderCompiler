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

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/microcom.h"
#include "dxclib/dxc.h"
#include <vector>
#include <string>

#include "dxc/dxcapi.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "llvm/Support/raw_ostream.h"

inline bool wcsieq(LPCWSTR a, LPCWSTR b) { return _wcsicmp(a, b) == 0; }

using namespace dxc;
using namespace llvm::opt;
using namespace hlsl::options;


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

int __cdecl wmain(int argc, const wchar_t **argv_) {
  if (FAILED(DxcInitThreadMalloc())) return 1;
  DxcSetThreadMallocToDefault();
  try {
    if (initHlslOptTable()) throw std::bad_alloc();

    // Parse command line options.
    const OptTable *optionTable = getHlslOptTable();
    MainArgs argStrings(argc, argv_);
    DxcOpts dxcOpts;
    DxcDllSupport dxcSupport;

    // Read options and check errors.
    {
      std::string errorString;
      llvm::raw_string_ostream errorStream(errorString);
      int optResult =
          ReadDxcOpts(optionTable, DxrFlags, argStrings, dxcOpts, errorStream);
      errorStream.flush();
      if (errorString.size()) {
        fprintf(stderr, "dxc failed : %s\n", errorString.data());
      }
      if (optResult != 0) {
        return optResult;
      }
    }

    // Apply defaults.
    if (dxcOpts.EntryPoint.empty() && !dxcOpts.RecompileFromBinary) {
      dxcOpts.EntryPoint = "main";
    }

    // Setup a helper DLL.
    {
      std::string dllErrorString;
      llvm::raw_string_ostream dllErrorStream(dllErrorString);
      int dllResult = SetupDxcDllSupport(dxcOpts, dxcSupport, dllErrorStream);
      dllErrorStream.flush();
      if (dllErrorString.size()) {
        fprintf(stderr, "%s\n", dllErrorString.data());
      }
      if (dllResult)
        return dllResult;
    }

    EnsureEnabled(dxcSupport);
    // Handle help request, which overrides any other processing.
    if (dxcOpts.ShowHelp) {
      std::string helpString;
      llvm::raw_string_ostream helpStream(helpString);
      std::string version;
      llvm::raw_string_ostream versionStream(version);
      WriteDxCompilerVersionInfo(versionStream,
        dxcOpts.ExternalLib.empty() ? (LPCSTR)nullptr : dxcOpts.ExternalLib.data(),
        dxcOpts.ExternalFn.empty() ? (LPCSTR)nullptr : dxcOpts.ExternalFn.data(),
        dxcSupport);
      versionStream.flush();
      optionTable->PrintHelp(helpStream, "dxr.exe", "HLSL Rewriter",
                             version.c_str(),
                             hlsl::options::RewriteOption,
                             (dxcOpts.ShowHelpHidden ? 0 : HelpHidden));
      helpStream.flush();
      WriteUtf8ToConsoleSizeT(helpString.data(), helpString.size());
      return 0;
    }

    CComPtr<IDxcRewriter2> pRewriter;
    CComPtr<IDxcOperationResult> pRewriteResult;
    CComPtr<IDxcBlobEncoding> pSource;
    std::wstring wName(CA2W(dxcOpts.InputFile.empty()? "" : dxcOpts.InputFile.data()));
    if (!dxcOpts.InputFile.empty()) {
      IFT_Data(FileMapDxcBlobEncoding::CreateForFile(wName.c_str(), &pSource), wName.c_str());
    }
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    IFT(dxcSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
    IFT(dxcSupport.CreateInstance(CLSID_DxcRewriter, &pRewriter));
    IFT(pRewriter->RewriteWithOptions(pSource, wName.c_str(),
                                      argv_, argc,
                                      nullptr, 0, pIncludeHandler,
                                      &pRewriteResult));

    if (dxcOpts.OutputObject.empty()) {
      // No -Fo, print to console
      WriteOperationResultToConsole(pRewriteResult, !dxcOpts.OutputWarnings);
    } else {
      WriteOperationErrorsToConsole(pRewriteResult, !dxcOpts.OutputWarnings);
      HRESULT hr;
      IFT(pRewriteResult->GetStatus(&hr));
      if (SUCCEEDED(hr)) {
        CA2W wOutputObject(dxcOpts.OutputObject.data());
        CComPtr<IDxcBlob> pObject;
        IFT(pRewriteResult->GetResult(&pObject));
        WriteBlobToFile(pObject, wOutputObject.m_psz, dxcOpts.DefaultTextCodePage);
        printf("Rewrite output: %s", dxcOpts.OutputObject.data());
      }
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
