///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxopt.cpp                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxopt console program.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include <vector>
#include <string>

#include "dxc/dxcapi.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/microcom.h"
#include <comdef.h>
#include <iostream>
#include <limits>

inline bool wcseq(LPCWSTR a, LPCWSTR b) {
  return (a == nullptr && b == nullptr) || (a != nullptr && b != nullptr && wcscmp(a, b) == 0);
}
inline bool wcsieq(LPCWSTR a, LPCWSTR b) { return _wcsicmp(a, b) == 0; }
inline bool wcsistarts(LPCWSTR text, LPCWSTR prefix) {
  return wcslen(text) >= wcslen(prefix) && _wcsnicmp(text, prefix, wcslen(prefix)) == 0;
}

enum class ProgramAction {
  PrintHelp,
  PrintPasses,
  PrintPassesWithDetails,
  RunOptimizer,
};

const wchar_t *STDIN_FILE_NAME = L"-";
bool isStdIn(LPCWSTR fName) {
  return wcscmp(STDIN_FILE_NAME, fName) == 0;
}

static HRESULT ReadStdin(std::string &input) {
  HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
  std::vector<unsigned char> buffer(1024);
  DWORD numBytesRead = -1;
  bool ok = false;

  // Read all data from stdin.
  while (ok = ReadFile(hStdIn, buffer.data(), buffer.size(), &numBytesRead, NULL)) {
    if (numBytesRead == 0)
      break;
    std::copy(buffer.begin(), buffer.begin() + numBytesRead, std::back_inserter(input));
  }

  DWORD lastError = GetLastError();

  // Make sure we reached EOF successfully.
  if (ok && numBytesRead == 0)
    return S_OK;
  // Or reached the end of a pipe.
  else if (!ok && numBytesRead == 0 && lastError == ERROR_BROKEN_PIPE)
    return S_OK;
  else
    return E_FAIL;
}

static void BlobFromFile(LPCWSTR pFileName, IDxcBlob **ppBlob) {
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcBlobEncoding> pFileBlob;
  IFT(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));
  if (isStdIn(pFileName)) {
    std::string input;
    IFT(ReadStdin(input));
    IFTBOOL(input.size() < std::numeric_limits<UINT32>::max(), E_FAIL);
    IFT(pLibrary->CreateBlobWithEncodingOnHeapCopy(input.data(), (UINT32)input.size(), CP_UTF8, &pFileBlob))
  }
  else {
    IFT(pLibrary->CreateBlobFromFile(pFileName, nullptr, &pFileBlob));
  }
  *ppBlob = pFileBlob.Detach();
}

static void PrintOptOutput(LPCWSTR pFileName, IDxcBlob *pBlob, IDxcBlobEncoding *pOutputText) {
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcBlobEncoding> pOutputText16;
  IFT(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));
  IFT(pLibrary->GetBlobAsUtf16(pOutputText, &pOutputText16));
  wprintf(L"%*s", (int)pOutputText16->GetBufferSize(),
          (wchar_t *)pOutputText16->GetBufferPointer());
  if (pBlob && pFileName && *pFileName) {
    dxc::WriteBlobToFile(pBlob, pFileName);
  }
}

static void PrintPasses(IDxcOptimizer *pOptimizer, bool includeDetails) {
  UINT32 passCount;
  IFT(pOptimizer->GetAvailablePassCount(&passCount));
  for (UINT32 i = 0; i < passCount; ++i) {
    CComPtr<IDxcOptimizerPass> pPass;
    CComHeapPtr<wchar_t> pName;
    IFT(pOptimizer->GetAvailablePass(i, &pPass));
    IFT(pPass->GetOptionName(&pName));
    if (!includeDetails) {
      wprintf(L"%s\n", pName.m_pData);
      continue;
    }

    CComHeapPtr<wchar_t> pDescription;
    IFT(pPass->GetDescription(&pDescription));
    if (*pDescription) {
      wprintf(L"%s - %s\n", pName.m_pData, pDescription.m_pData);
    }
    else {
      wprintf(L"%s\n", pName.m_pData);
    }
    UINT32 argCount;
    IFT(pPass->GetOptionArgCount(&argCount));
    if (argCount) {
      wprintf(L"%s", L"Options:\n");
      for (UINT32 argIdx = 0; argIdx < argCount; ++argIdx) {
        CComHeapPtr<wchar_t> pArgName;
        CComHeapPtr<wchar_t> pArgDescription;
        IFT(pPass->GetOptionArgName(argIdx, &pArgName));
        IFT(pPass->GetOptionArgDescription(argIdx, &pArgDescription));
        if (pArgDescription.m_pData && *pArgDescription.m_pData && !wcsieq(L"None", pArgDescription.m_pData)) {
          wprintf(L"  %s - %s\n", pArgName.m_pData, pArgDescription.m_pData);
        }
        else {
          wprintf(L"  %s\n", pArgName.m_pData);
        }
      }
      wprintf(L"%s", L"\n");
    }
  }
}

static void PrintHelp() {
  wprintf(L"%s",
    L"Performs optimizations on a bitcode file by running a sequence of passes.\n\n"
    L"dxopt [-? | -passes | -pass-details | IN-FILE [-o=OUT-FILE] OPT-ARGUMENTS ...]\n\n"
    L"Arguments:\n"
    L"  -?  Displays this help message\n"
    L"  -passes        Displays a list of pass names\n"
    L"  -pass-details  Displays a list of passes with detailed information\n"
    L"  IN-FILE        File with with bitcode to optimize\n"
    L"  -o=OUT-FILE    Output file for processed module\n"
    L"  OPT-ARGUMENTS  One or more passes to run in sequence\n"
    L"\n"
    L"Text that is traced during optimization is written to the standard output.\n"
  );
}

int __cdecl wmain(int argc, const wchar_t **argv_) {
  const char *pStage = "Operation";
  int retVal = 0;
  try {
    // Parse command line options.
    pStage = "Argument processing";

    ProgramAction action = ProgramAction::PrintHelp;
    LPCWSTR inFileName = nullptr;
    LPCWSTR outFileName = nullptr;
    const wchar_t **optArgs = nullptr;
    UINT32 optArgCount = 0;

    if (argc > 1) {
      int argIdx = 1;
      LPCWSTR arg = argv_[1];
      if (wcsieq(arg, L"-?") || wcsieq(arg, L"/?")) {
        action = ProgramAction::PrintHelp;
      }
      else if (wcsieq(arg, L"-passes") || wcsieq(arg, L"/passes")) {
        action = ProgramAction::PrintPasses;
      }
      else if (wcsieq(arg, L"-pass-details") || wcsieq(arg, L"/pass-details")) {
        action = ProgramAction::PrintPassesWithDetails;
      }
      else {
        action = ProgramAction::RunOptimizer;
        // Next arg does not start with '-' and so is a filename,
        // or next arg equals '-' so we should read from stdin.
        if (!wcsistarts(arg, L"-") || isStdIn(arg)) {
          inFileName = arg;
          argIdx++;
        }
        // No filename argument give so read from stdin.
        else {
          inFileName = STDIN_FILE_NAME;
        }
        // Look for a file output argument.
        if (argc > argIdx && wcsistarts(argv_[argIdx], L"-o=")) {
          outFileName = argv_[argIdx] + 3;
          argIdx++;
        }
        // The remaining arguments are optimizer args.
        optArgs = argv_ + argIdx;
        optArgCount = argc - argIdx;
      }
    }

    if (action == ProgramAction::PrintHelp) {
      PrintHelp();
      return retVal;
    }

    CComPtr<IDxcBlob> pBlob;
    CComPtr<IDxcBlob> pOutputModule;
    CComPtr<IDxcBlobEncoding> pOutputText;
    CComPtr<IDxcOptimizer> pOptimizer;
    IFT(DxcCreateInstance(CLSID_DxcOptimizer, IID_PPV_ARGS(&pOptimizer)));
    switch (action) {
    case ProgramAction::PrintPasses:
      pStage = "Printing passes...";
      PrintPasses(pOptimizer, false);
      break;
    case ProgramAction::PrintPassesWithDetails:
      pStage = "Printing pass details...";
      PrintPasses(pOptimizer, true);
      break;
    case ProgramAction::RunOptimizer:
      pStage = "Optimizer processing";
      BlobFromFile(inFileName, &pBlob);
      IFT(pOptimizer->RunOptimizer(pBlob, optArgs, optArgCount, &pOutputModule, &pOutputText));
      PrintOptOutput(outFileName, pOutputModule, pOutputText);
      break;
    }
  } catch (const ::hlsl::Exception &hlslException) {
    try {
      const char *msg = hlslException.what();
      Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as
                                          // UTF-8 because we use ASCII only errors
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer),
                  "Operation failed - error code 0x%08x.\n", hlslException.hr);
        msg = printBuffer;
      }

      dxc::WriteUtf8ToConsoleSizeT(msg, strlen(msg));
      printf("\n");
    } catch (...) {
      printf("%s failed - unable to retrieve error message.\n", pStage);
    }

    return 1;
  } catch (std::bad_alloc &) {
    printf("%s failed - out of memory.\n", pStage);
    return 1;
  } catch (...) {
    printf("%s failed - unknown error.\n", pStage);
    return 1;
  }

  return retVal;
}
