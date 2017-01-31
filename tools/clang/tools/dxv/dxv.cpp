///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxv.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxv console program.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/dxcapi.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"

#include "llvm/Support/CommandLine.h"

using namespace dxc;
using namespace llvm;
using namespace llvm::opt;
using namespace hlsl::options;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input dxil file>"), cl::init("-"));

class DxvContext {
private:
  DxcDllSupport &m_dxcSupport;
public:
  DxvContext(DxcDllSupport &dxcSupport)
      : m_dxcSupport(dxcSupport) {}

  void Validate();
};

void DxvContext::Validate() {
  CComPtr<IDxcOperationResult> pCompileResult;

  {
    CComPtr<IDxcBlobEncoding> pSource;
    ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);

    CComPtr<IDxcAssembler> pAssembler;
    CComPtr<IDxcOperationResult> pAsmResult;

    CComPtr<IDxcBlob> pContainerBlob;
    HRESULT resultStatus;

    IFT(m_dxcSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    IFT(pAssembler->AssembleToContainer(pSource, &pAsmResult));
    IFT(pAsmResult->GetStatus(&resultStatus));
    if (FAILED(resultStatus)) {
      CComPtr<IDxcBlobEncoding> text;
      IFT(pAsmResult->GetErrorBuffer(&text));
      const char *pStart = (const char *)text->GetBufferPointer();
      std::string msg(pStart);
      IFTMSG(resultStatus, msg);
      return;
    }
    IFT(pAsmResult->GetResult(&pContainerBlob));

    CComPtr<IDxcValidator> pValidator;
    CComPtr<IDxcOperationResult> pResult;

    IFT(m_dxcSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
    IFT(pValidator->Validate(pContainerBlob, DxcValidatorFlags_InPlaceEdit, &pResult));

    HRESULT status;
    IFT(pResult->GetStatus(&status));

    if (FAILED(status)) {
      CComPtr<IDxcBlobEncoding> text;
      IFT(pResult->GetErrorBuffer(&text));
      const char *pStart = (const char *)text->GetBufferPointer();
      std::string msg(pStart);
      IFTMSG(status, msg);
    } else {
      printf("Validation succeed.");
    }
  }
}

int __cdecl main(int argc,  _In_reads_z_(argc) const char **argv) {
  const char *pStage = "Operation";
  try {
    pStage = "Argument processing";

    // Parse command line options.
    cl::ParseCommandLineOptions(argc, argv, "dxil validator\n");

    DxcDllSupport dxcSupport;
    dxc::EnsureEnabled(dxcSupport);

    DxvContext context(dxcSupport);
    pStage = "Validation";
    context.Validate();
  } catch (const ::hlsl::Exception &hlslException) {
    try {
      const char *msg = hlslException.what();
      Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as
                                          // UTF-8 because we use ASCII only errors
                                          // only
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer),
                  "Validation failed - error code 0x%08x.", hlslException.hr);
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
