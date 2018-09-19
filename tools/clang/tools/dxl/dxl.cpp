///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxl.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxl console program.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/dxcapi.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/HLSL/DxilContainer.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support//MSFileSystem.h"
#include "llvm/Support/FileSystem.h"
#include <dia2.h>
#include <intsafe.h>

using namespace llvm;
using namespace llvm::opt;
using namespace dxc;
using namespace hlsl::options;

static cl::opt<bool> Help("h", cl::desc("Alias for -help"), cl::Hidden);

static cl::opt<std::string>
    InputFiles(cl::Positional, cl::desc("<input .lib files seperate with ;>"),
               cl::init(""));

static cl::opt<std::string> EntryName("E", cl::desc("Entry function name"),
                                      cl::value_desc("entryfunction"),
                                      cl::init("main"));

static cl::opt<std::string> TargetProfile("T", cl::desc("Target profile"),
                                          cl::value_desc("profile"),
                                          cl::init("ps_6_1"));

static cl::opt<std::string> OutputFilename("Fo",
                                           cl::desc("Override output filename"),
                                           cl::value_desc("filename"));


class DxlContext {

private:
  DxcDllSupport &m_dxcSupport;
  template <typename TInterface>
  HRESULT CreateInstance(REFCLSID clsid, _Outptr_ TInterface **pResult) {
    return m_dxcSupport.CreateInstance(clsid, pResult);
  }

public:
  DxlContext(DxcDllSupport &dxcSupport) : m_dxcSupport(dxcSupport) {}

  int Link();
};

int DxlContext::Link() {
  std::string entry = EntryName;
  std::string profile = TargetProfile;

  CComPtr<IDxcLinker> pLinker;
  IFT(CreateInstance(CLSID_DxcLinker, &pLinker));

  StringRef InputFilesRef(InputFiles);
  SmallVector<StringRef, 2> InputFileList;
  InputFilesRef.split(InputFileList, ";");

  std::vector<std::wstring> wInputFiles;
  wInputFiles.reserve(InputFileList.size());
  std::vector<LPCWSTR> wpInputFiles;
  wpInputFiles.reserve(InputFileList.size());
  for (auto &file : InputFileList) {
    wInputFiles.emplace_back(StringRefUtf16(file.str()));
    wpInputFiles.emplace_back(wInputFiles.back().c_str());
    CComPtr<IDxcBlobEncoding> pLib;
    ReadFileIntoBlob(m_dxcSupport, wInputFiles.back().c_str(), &pLib);
    IFT(pLinker->RegisterLibrary(wInputFiles.back().c_str(), pLib));
  }

  CComPtr<IDxcOperationResult> pLinkResult;

  IFT(pLinker->Link(StringRefUtf16(entry), StringRefUtf16(profile),
                wpInputFiles.data(), wpInputFiles.size(), nullptr, 0,
                &pLinkResult));

  HRESULT status;
  IFT(pLinkResult->GetStatus(&status));
  if (SUCCEEDED(status)) {
    CComPtr<IDxcBlob> pContainer;
    IFT(pLinkResult->GetResult(&pContainer));
    if (pContainer.p != nullptr) {
      // Infer the output filename if needed.
      if (OutputFilename.empty()) {
        OutputFilename = EntryName + ".dxbc";
      }
      WriteBlobToFile(pContainer, StringRefUtf16(OutputFilename));
    }
  } else {
    CComPtr<IDxcBlobEncoding> pErrors;
    IFT(pLinkResult->GetErrorBuffer(&pErrors));
    if (pErrors != nullptr) {
      printf("Link failed:\n%s",
             static_cast<char *>(pErrors->GetBufferPointer()));
    }
  }
  return status;
}

using namespace hlsl::options;

int __cdecl main(int argc, _In_reads_z_(argc) char **argv) {
  const char *pStage = "Operation";
  int retVal = 0;
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    return 1;
  llvm::sys::fs::AutoCleanupPerThreadFileSystem auto_cleanup_fs;
  if (FAILED(DxcInitThreadMalloc())) return 1;
  DxcSetThreadMallocOrDefault(nullptr);
  try {
    llvm::sys::fs::MSFileSystem *msfPtr;
    IFT(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    pStage = "Argument processing";

    // Parse command line options.
    cl::ParseCommandLineOptions(argc, argv, "dxil linker\n");

    if (InputFiles == "" || Help) {
      cl::PrintHelpMessage();
      return 2;
    }

    DxcDllSupport dxcSupport;

    // Read options and check errors.
    dxc::EnsureEnabled(dxcSupport);
    DxlContext context(dxcSupport);

    pStage = "Linking";
    retVal = context.Link();
  } catch (const ::hlsl::Exception &hlslException) {
    try {
      const char *msg = hlslException.what();
      Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as
                                          // UTF-8 because we use ASCII only errors
                                          // only
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer),
                  "Link failed - error code 0x%08x.", hlslException.hr);
        msg = printBuffer;
      }
      printf("%s\n", msg);
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