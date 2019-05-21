///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcTestUtils.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Utility function implementations for testing dxc APIs                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "CompilationResult.h"
#include "DxcTestUtils.h"
#include "HlslTestUtils.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Global.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/FileSystem.h"

using namespace std;
using namespace hlsl_test;

MODULE_SETUP(TestModuleSetup);
MODULE_CLEANUP(TestModuleCleanup);

bool TestModuleSetup() {
  // Use this module-level function to set up LLVM dependencies.
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    return false;
  if (FAILED(DxcInitThreadMalloc()))
    return false;
  DxcSetThreadMallocToDefault();

  if (hlsl::options::initHlslOptTable()) {
    return false;
  }
  return true;
}

bool TestModuleCleanup() {
  // Use this module-level function to set up LLVM dependencies.
  // In particular, clean up managed static allocations used by
  // parsing options with the LLVM library.
  ::hlsl::options::cleanupHlslOptTable();
  ::llvm::llvm_shutdown();
  DxcClearThreadMalloc();
  DxcCleanupThreadMalloc();
  llvm::sys::fs::CleanupPerThreadFileSystem();
  return true;
}

std::shared_ptr<HlslIntellisenseSupport> CompilationResult::DefaultHlslSupport;

void CheckOperationSucceeded(IDxcOperationResult *pResult, IDxcBlob **ppBlob) {
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(ppBlob));
}

static bool CheckMsgs(llvm::StringRef text, llvm::ArrayRef<LPCSTR> pMsgs,
                      bool bRegex) {
  const char *pStart = !text.empty() ? text.begin() : nullptr;
  const char *pEnd = !text.empty() ? text.end() : nullptr;
  for (auto pMsg : pMsgs) {
    if (bRegex) {
      llvm::Regex RE(pMsg);
      std::string reErrors;
      VERIFY_IS_TRUE(RE.isValid(reErrors));
      if (!RE.match(text)) {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"Unable to find regex '%S' in text:\r\n%.*S", pMsg, (pEnd - pStart),
          pStart));
        VERIFY_IS_TRUE(false);
      }
    } else {
      const char *pMatch = std::search(pStart, pEnd, pMsg, pMsg + strlen(pMsg));
      if (pEnd == pMatch) {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(
            L"Unable to find '%S' in text:\r\n%.*S", pMsg, (pEnd - pStart),
            pStart));
      }
      VERIFY_IS_FALSE(pEnd == pMatch);
    }
  }
  return true;
}

bool CheckMsgs(const LPCSTR pText, size_t TextCount, const LPCSTR *pErrorMsgs,
               size_t errorMsgCount, bool bRegex) {
  return CheckMsgs(llvm::StringRef(pText, TextCount),
                   llvm::ArrayRef<LPCSTR>(pErrorMsgs, errorMsgCount), bRegex);
}

static bool CheckNotMsgs(llvm::StringRef text, llvm::ArrayRef<LPCSTR> pMsgs,
                         bool bRegex) {
  const char *pStart = !text.empty() ? text.begin() : nullptr;
  const char *pEnd = !text.empty() ? text.end() : nullptr;
  for (auto pMsg : pMsgs) {
    if (bRegex) {
      llvm::Regex RE(pMsg);
      std::string reErrors;
      VERIFY_IS_TRUE(RE.isValid(reErrors));
      if (RE.match(text)) {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"Unexpectedly found regex '%S' in text:\r\n%.*S", pMsg, (pEnd - pStart),
          pStart));
        VERIFY_IS_TRUE(false);
      }
    }
    else {
      const char *pMatch = std::search(pStart, pEnd, pMsg, pMsg + strlen(pMsg));
      if (pEnd != pMatch) {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"Unexpectedly found '%S' in text:\r\n%.*S", pMsg, (pEnd - pStart),
          pStart));
      }
      VERIFY_IS_TRUE(pEnd == pMatch);
    }
  }
  return true;
}

bool CheckNotMsgs(const LPCSTR pText, size_t TextCount, const LPCSTR *pErrorMsgs,
                  size_t errorMsgCount, bool bRegex) {
  return CheckNotMsgs(llvm::StringRef(pText, TextCount),
    llvm::ArrayRef<LPCSTR>(pErrorMsgs, errorMsgCount), bRegex);
}

bool CheckOperationResultMsgs(IDxcOperationResult *pResult,
                              llvm::ArrayRef<LPCSTR> pErrorMsgs,
                              bool maySucceedAnyway, bool bRegex) {
  HRESULT status;
  CComPtr<IDxcBlobEncoding> text;
  if (!pResult)
    return true;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&text));
  const char *pStart = text ? (const char *)text->GetBufferPointer() : nullptr;
  const char *pEnd = text ? pStart + text->GetBufferSize() : nullptr;
  if (pErrorMsgs.empty() || (pErrorMsgs.size() == 1 && !pErrorMsgs[0])) {
    if (FAILED(status) && pStart) {
      WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"Expected success but found errors\r\n%.*S", (pEnd - pStart),
          pStart));
    }
    VERIFY_SUCCEEDED(status);
  } else {
    if (SUCCEEDED(status) && maySucceedAnyway) {
      return false;
    }
    CheckMsgs(llvm::StringRef((const char *)text->GetBufferPointer(),
                              text->GetBufferSize()),
              pErrorMsgs, bRegex);
  }
  return true;
}

bool CheckOperationResultMsgs(IDxcOperationResult *pResult,
                              const LPCSTR *pErrorMsgs, size_t errorMsgCount,
                              bool maySucceedAnyway, bool bRegex) {
  return CheckOperationResultMsgs(
      pResult, llvm::ArrayRef<LPCSTR>(pErrorMsgs, errorMsgCount),
      maySucceedAnyway, bRegex);
}

std::string DisassembleProgram(dxc::DxcDllSupport &dllSupport,
                               IDxcBlob *pProgram) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pDisassembly;

  if (!dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(dllSupport.Initialize());
  }

  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
  return BlobToUtf8(pDisassembly);
}

void AssembleToContainer(dxc::DxcDllSupport &dllSupport, IDxcBlob *pModule,
                         IDxcBlob **pContainer) {
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcOperationResult> pResult;
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pModule, &pResult));
  CheckOperationSucceeded(pResult, pContainer);
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions to deal with passes.

void SplitPassList(LPWSTR pPassesBuffer, std::vector<LPCWSTR> &passes) {
  while (*pPassesBuffer) {
    // Skip comment lines.
    if (*pPassesBuffer == L'#') {
      while (*pPassesBuffer && *pPassesBuffer != '\n' &&
             *pPassesBuffer != '\r') {
        ++pPassesBuffer;
      }
      while (*pPassesBuffer == '\n' || *pPassesBuffer == '\r') {
        ++pPassesBuffer;
      }
      continue;
    }
    // Every other line is an option. Find the end of the line/buffer and
    // terminate it.
    passes.push_back(pPassesBuffer);
    while (*pPassesBuffer && *pPassesBuffer != '\n' && *pPassesBuffer != '\r') {
      ++pPassesBuffer;
    }
    while (*pPassesBuffer == '\n' || *pPassesBuffer == '\r') {
      *pPassesBuffer = L'\0';
      ++pPassesBuffer;
    }
  }
}

std::wstring BlobToUtf16(_In_ IDxcBlob *pBlob) {
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  const UINT CP_UTF16 = 1200;
  IFT(pBlob->QueryInterface(&pBlobEncoding));
  BOOL known;
  UINT32 codePage;
  IFT(pBlobEncoding->GetEncoding(&known, &codePage));
  std::wstring result;
  if (codePage == CP_UTF16) {
    result.resize(pBlob->GetBufferSize() + 1);
    memcpy((void *)result.data(), pBlob->GetBufferPointer(),
           pBlob->GetBufferSize());
    return result;
  } else if (codePage == CP_UTF8) {
    Unicode::UTF8ToUTF16String((char *)pBlob->GetBufferPointer(),
                               pBlob->GetBufferSize(), &result);
    return result;
  } else {
    throw std::runtime_error("Unsupported codepage.");
  }
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const char *pVal,
                _Outptr_ IDxcBlobEncoding **ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(pVal, strlen(pVal), CP_UTF8,
                                                ppBlob));
}

void MultiByteStringToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val,
                           UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(val.data(), val.size(),
                                                codePage, ppBlob));
}

void MultiByteStringToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val,
                           UINT32 codePage, _Outptr_ IDxcBlob **ppBlob) {
  MultiByteStringToBlob(dllSupport, val, codePage, (IDxcBlobEncoding **)ppBlob);
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val,
                _Outptr_ IDxcBlobEncoding **ppBlob) {
  MultiByteStringToBlob(dllSupport, val, CP_UTF8, ppBlob);
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val,
                _Outptr_ IDxcBlob **ppBlob) {
  Utf8ToBlob(dllSupport, val, (IDxcBlobEncoding **)ppBlob);
}

void Utf16ToBlob(dxc::DxcDllSupport &dllSupport, const std::wstring &val,
                 _Outptr_ IDxcBlobEncoding **ppBlob) {
  const UINT32 CP_UTF16 = 1200;
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(
      val.data(), val.size() * sizeof(wchar_t), CP_UTF16, ppBlob));
}

void Utf16ToBlob(dxc::DxcDllSupport &dllSupport, const std::wstring &val,
                 _Outptr_ IDxcBlob **ppBlob) {
  Utf16ToBlob(dllSupport, val, (IDxcBlobEncoding **)ppBlob);
}

void VerifyCompileOK(dxc::DxcDllSupport &dllSupport, LPCSTR pText,
                     LPWSTR pTargetProfile, LPCWSTR pArgs,
                     _Outptr_ IDxcBlob **ppResult) {
  std::vector<std::wstring> argsW;
  std::vector<LPCWSTR> args;
  if (pArgs) {
    wistringstream argsS(pArgs);
    copy(istream_iterator<wstring, wchar_t>(argsS),
         istream_iterator<wstring, wchar_t>(), back_inserter(argsW));
    transform(argsW.begin(), argsW.end(), back_inserter(args),
              [](const wstring &w) { return w.data(); });
  }
  VerifyCompileOK(dllSupport, pText, pTargetProfile, args, ppResult);
}

void VerifyCompileOK(dxc::DxcDllSupport &dllSupport, LPCSTR pText,
                     LPWSTR pTargetProfile, std::vector<LPCWSTR> &args,
                     _Outptr_ IDxcBlob **ppResult) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcOperationResult> pResult;
  HRESULT hrCompile;
  *ppResult = nullptr;
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  Utf8ToBlob(dllSupport, pText, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      pTargetProfile, args.data(), args.size(),
                                      nullptr, 0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrCompile));
  VERIFY_SUCCEEDED(hrCompile);
  VERIFY_SUCCEEDED(pResult->GetResult(ppResult));
}

// VersionSupportInfo Implementation
VersionSupportInfo::VersionSupportInfo()
    : m_CompilerIsDebugBuild(false), m_InternalValidator(false), m_DxilMajor(0),
      m_DxilMinor(0), m_ValMajor(0), m_ValMinor(0) {}

void VersionSupportInfo::Initialize(dxc::DxcDllSupport &dllSupport) {
  VERIFY_IS_TRUE(dllSupport.IsEnabled());

  // Default to Dxil 1.0 and internal Val 1.0
  m_DxilMajor = m_ValMajor = 1;
  m_DxilMinor = m_ValMinor = 0;
  m_InternalValidator = true;
  CComPtr<IDxcVersionInfo> pVersionInfo;
  UINT32 VersionFlags = 0;

  // If the following fails, we have Dxil 1.0 compiler
  if (SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pVersionInfo))) {
    VERIFY_SUCCEEDED(pVersionInfo->GetVersion(&m_DxilMajor, &m_DxilMinor));
    VERIFY_SUCCEEDED(pVersionInfo->GetFlags(&VersionFlags));
    m_CompilerIsDebugBuild =
        (VersionFlags & DxcVersionInfoFlags_Debug) ? true : false;
    pVersionInfo.Release();
  }

  if (SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcValidator, &pVersionInfo))) {
    VERIFY_SUCCEEDED(pVersionInfo->GetVersion(&m_ValMajor, &m_ValMinor));
    VERIFY_SUCCEEDED(pVersionInfo->GetFlags(&VersionFlags));
    if (m_ValMinor > 0) {
      // flag only exists on newer validator, assume internal otherwise.
      m_InternalValidator =
          (VersionFlags & DxcVersionInfoFlags_Internal) ? true : false;
    } else {
      // With old compiler, validator is the only way to get this
      m_CompilerIsDebugBuild =
          (VersionFlags & DxcVersionInfoFlags_Debug) ? true : false;
    }
  } else {
    // If create instance of IDxcVersionInfo on validator failed, we have an old
    // validator from dxil.dll
    m_InternalValidator = false;
  }
}
bool VersionSupportInfo::SkipIRSensitiveTest() {
  // Only debug builds preserve BB names.
  if (!m_CompilerIsDebugBuild) {
    WEX::Logging::Log::Comment(
        L"Test skipped due to name preservation requirement.");
    return true;
  }
  return false;
}
bool VersionSupportInfo::SkipDxilVersion(unsigned major, unsigned minor) {
  if (m_DxilMajor < major || (m_DxilMajor == major && m_DxilMinor < minor) ||
      m_ValMajor < major || (m_ValMajor == major && m_ValMinor < minor)) {
    WEX::Logging::Log::Comment(WEX::Common::String().Format(
        L"Test skipped because it requires Dxil %u.%u and Validator %u.%u.",
        major, minor, major, minor));
    return true;
  }
  return false;
}
bool VersionSupportInfo::SkipOutOfMemoryTest() { return false; }
