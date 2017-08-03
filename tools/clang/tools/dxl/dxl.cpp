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
#include <dia2.h>
#include <intsafe.h>

using namespace llvm;
using namespace llvm::opt;
using namespace dxc;
using namespace hlsl::options;

static cl::opt<std::string> InputFiles(cl::Positional, cl::Required,
                                        cl::desc("<input .lib files>"));

static cl::opt<std::string> EntryName("E", cl::desc("Entry function name"),
                                      cl::value_desc("entryfunction"),
                                      cl::init("main"));

static cl::opt<std::string> TargetProfile("T", cl::Required,
                                          cl::desc("Target profile"),
                                          cl::value_desc("profile"));

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

  void Link();
};

void DxlContext::Link() {
  std::string entry = EntryName;
  std::string profile = TargetProfile;

  CComPtr<IDxcLinker> pLinker;
  IFT(CreateInstance(CLSID_DxcLinker, &pLinker));

  StringRef InputFilesRef(InputFiles);
  SmallVector<StringRef, 2> InputFileList;
  InputFilesRef.split(InputFileList, ";");

  std::vector<std::wstring> wInputFiles;
  std::vector<LPCWSTR> wpInputFiles;
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
  }
}

using namespace hlsl::options;

int __cdecl main(int argc, _In_reads_z_(argc) char **argv) {
  const char *pStage = "Operation";
  try {
    pStage = "Argument processing";

    // Parse command line options.
    cl::ParseCommandLineOptions(argc, argv, "dxil linker\n");

    DxcDllSupport dxcSupport;

    // Read options and check errors.
    dxc::EnsureEnabled(dxcSupport);
    DxlContext context(dxcSupport);

    pStage = "Linking";
    context.Link();
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

  return 0;
}