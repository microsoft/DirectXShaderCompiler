///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxa.cpp                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides the entry point for the dxa console program.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/dxcapi.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support//MSFileSystem.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <dia2.h>
#include <intsafe.h>

using namespace llvm;
using namespace llvm::opt;
using namespace dxc;
using namespace hlsl::options;

static cl::opt<bool> Help("help", cl::desc("Print help"));
static cl::alias Help_h("h", cl::aliasopt(Help));
static cl::alias Help_q("?", cl::aliasopt(Help));

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input .llvm file>"));

static cl::opt<std::string> OutputFilename("o",
                                           cl::desc("Override output filename"),
                                           cl::value_desc("filename"));

static cl::opt<bool> ListParts("listparts",
                               cl::desc("List parts in input container"),
                               cl::init(false));

static cl::opt<std::string>
    ExtractPart("extractpart", cl::desc("Extract one part from input container (use 'module' or 'dbgmodule' for a .ll file)"));

static cl::opt<bool> ListFiles("listfiles",
                               cl::desc("List files in input container"),
                               cl::init(false));
static cl::opt<std::string>
    ExtractFile("extractfile", cl::desc("Extract file from debug information (use '*' for all files)"));

static cl::opt<bool> DumpRootSig("dumprs",
                               cl::desc("Dump root signature"),
                               cl::init(false));

class DxaContext {

private:
  DxcDllSupport &m_dxcSupport;
  HRESULT FindModule(hlsl::DxilFourCC fourCC, IDxcBlob *pSource, IDxcLibrary *pLibrary, IDxcBlob **ppTarget);
  bool ExtractPart(uint32_t Part, IDxcBlob **ppTargetBlob);
public:
  DxaContext(DxcDllSupport &dxcSupport) : m_dxcSupport(dxcSupport) {}

  void Assemble();
  bool ExtractFile(const char *pName);
  bool ExtractPart(const char *pName);
  void ListFiles();
  void ListParts();
  void DumpRS();
};

void DxaContext::Assemble() {
  CComPtr<IDxcOperationResult> pAssembleResult;

  {
    CComPtr<IDxcBlobEncoding> pSource;
    ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);

    CComPtr<IDxcAssembler> pAssembler;
    IFT(m_dxcSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    IFT(pAssembler->AssembleToContainer(pSource, &pAssembleResult));
  }

  CComPtr<IDxcBlobEncoding> pErrors;
  CComPtr<IDxcBlobUtf8> pErrorsUtf8;
  pAssembleResult->GetErrorBuffer(&pErrors);
  if (pErrors && pErrors->GetBufferSize() > 1) {
    IFT(pErrors->QueryInterface(IID_PPV_ARGS(&pErrorsUtf8)));
    printf("Errors or warnings:\n%s", pErrorsUtf8->GetStringPointer());
  }

  HRESULT status;
  IFT(pAssembleResult->GetStatus(&status));
  if (SUCCEEDED(status)) {
    printf("Assembly succeeded.\n");
    CComPtr<IDxcBlob> pContainer;
    IFT(pAssembleResult->GetResult(&pContainer));
    if (pContainer.p != nullptr) {
      // Infer the output filename if needed.
      if (OutputFilename.empty()) {
        if (InputFilename == "-") {
          OutputFilename = "-";
        } else {
          StringRef IFN = InputFilename;
          OutputFilename = (IFN.endswith(".ll") ? IFN.drop_back(3) : IFN).str();
          OutputFilename = (IFN.endswith(".bc") ? IFN.drop_back(3) : IFN).str();
          OutputFilename += ".dxbc";
        }
      }

      WriteBlobToFile(pContainer, StringRefUtf16(OutputFilename), DXC_CP_ACP);
      printf("Output written to \"%s\"\n", OutputFilename.c_str());
    }
  } else {
    printf("Assembly failed.\n");
  }
}

// Finds DXIL module from the blob assuming blob is either DxilContainer, DxilPartHeader, or DXIL module
HRESULT DxaContext::FindModule(hlsl::DxilFourCC fourCC, IDxcBlob *pSource, IDxcLibrary *pLibrary, IDxcBlob **ppTargetBlob) {
  if (!pSource || !pLibrary || !ppTargetBlob)
    return E_INVALIDARG;
  const UINT32 BC_C0DE = ((INT32)(INT8)'B' | (INT32)(INT8)'C' << 8 | (INT32)0xDEC0 << 16); // BC0xc0de in big endian
  const char *pBitcode = nullptr;
  const hlsl::DxilPartHeader *pDxilPartHeader = (hlsl::DxilPartHeader*)pSource->GetBufferPointer(); // Initialize assuming that source is starting with DXIL part

  if (BC_C0DE == *(UINT32*)pSource->GetBufferPointer()) {
    *ppTargetBlob = pSource;
    pSource->AddRef();
    return S_OK;
  }
  if (hlsl::IsValidDxilContainer((hlsl::DxilContainerHeader*)pSource->GetBufferPointer(), pSource->GetBufferSize())) {
    hlsl::DxilContainerHeader *pDxilContainerHeader = (hlsl::DxilContainerHeader*)pSource->GetBufferPointer();
    pDxilPartHeader = *std::find_if(begin(pDxilContainerHeader), end(pDxilContainerHeader), hlsl::DxilPartIsType(fourCC));
  }
  if (fourCC == pDxilPartHeader->PartFourCC) {
    UINT32 pBlobSize;
    hlsl::DxilProgramHeader *pDxilProgramHeader = (hlsl::DxilProgramHeader*)(pDxilPartHeader + 1);
    hlsl::GetDxilProgramBitcode(pDxilProgramHeader, &pBitcode, &pBlobSize);
    UINT32 offset = (UINT32)(pBitcode - (const char *)pSource->GetBufferPointer());
    pLibrary->CreateBlobFromBlob(pSource, offset, pBlobSize, ppTargetBlob);
    return S_OK;
  }
  return E_INVALIDARG;
}

void DxaContext::ListFiles() {
  CComPtr<IDxcBlobEncoding> pSource;
  ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);

  CComPtr<IDxcPdbUtils> pPdbUtils;
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcPdbUtils, &pPdbUtils));
  IFT(pPdbUtils->Load(pSource));

  UINT32 uNumSources = 0;
  IFT(pPdbUtils->GetSourceCount(&uNumSources));

  for (UINT32 i = 0; i < uNumSources; i++) {
    CComBSTR name;
    IFT(pPdbUtils->GetSourceName(i, &name));
    printf("%S\r\n", (LPWSTR)name);
  }
}

bool DxaContext::ExtractFile(const char *pName) {
  CComPtr<IDxcBlobEncoding> pSource;
  ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);

  CComPtr<IDxcPdbUtils> pPdbUtils;
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcPdbUtils, &pPdbUtils));
  IFT(pPdbUtils->Load(pSource));

  UINT32 uNumSources = 0;
  IFT(pPdbUtils->GetSourceCount(&uNumSources));
  bool printedAny = false;

  CA2W WideName(pName, CP_UTF8);
  for (UINT32 i = 0; i < uNumSources; i++) {
    CComBSTR name;
    IFT(pPdbUtils->GetSourceName(i, &name));
    if (strcmp("*", pName) == 0 || wcscmp((LPWSTR)name, WideName) == 0) {
      printedAny = true;
      CComPtr<IDxcBlobEncoding> pFileContent;
      IFT(pPdbUtils->GetSource(i, &pFileContent));
      printf("%.*s", (int)pFileContent->GetBufferSize(), (char *)pFileContent->GetBufferPointer());
    }
  }

  return printedAny;
}

bool DxaContext::ExtractPart(uint32_t PartKind, IDxcBlob **ppTargetBlob) {
  CComPtr<IDxcContainerReflection> pReflection;
  CComPtr<IDxcBlobEncoding> pSource;
  UINT32 partCount;
  ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  IFT(pReflection->Load(pSource));
  IFT(pReflection->GetPartCount(&partCount));

    for (UINT32 i = 0; i < partCount; ++i) {
    UINT32 curPartKind;
    IFT(pReflection->GetPartKind(i, &curPartKind));
    if (curPartKind == PartKind) {
      CComPtr<IDxcBlob> pContent;
      IFT(pReflection->GetPartContent(i, ppTargetBlob));
      return true;
    }
  }
  return false;

}

bool DxaContext::ExtractPart(const char *pName) {
  // If the part name is 'module', don't just extract the part,
  // but also skip the appropriate header.
  bool extractModule = strcmp("module", pName) == 0;
  if (extractModule) {
    pName = "DXIL";
  }
  if (strcmp("dbgmodule", pName) == 0) {
    pName = "ILDB";
    extractModule = true;
  }

  IFTARG(strlen(pName) == 4);

  const UINT32 matchName =
      ((UINT32)pName[0] | ((UINT32)pName[1] << 8) | ((UINT32)pName[2] << 16) |
       ((UINT32)pName[3] << 24));
  CComPtr<IDxcBlob> pContent;
  if (!ExtractPart(matchName, &pContent))
    return false;

  if (OutputFilename.empty()) {
    if (InputFilename == "-") {
      OutputFilename = "-";
    } else {
      OutputFilename = InputFilename.getValue();
      OutputFilename += ".";
      if (extractModule) {
        OutputFilename += "ll";
      } else {
        OutputFilename += pName;
      }
    }
  }

  if (extractModule) {
    char *pDxilPart = (char *)pContent->GetBufferPointer();
    hlsl::DxilProgramHeader *pProgramHdr = (hlsl::DxilProgramHeader *)pDxilPart;
    const char *pBitcode;
    uint32_t bitcodeLength;
    GetDxilProgramBitcode(pProgramHdr, &pBitcode, &bitcodeLength);
    uint32_t offset = pBitcode - pDxilPart;

    CComPtr<IDxcLibrary> pLib;
    CComPtr<IDxcBlob> pModuleBlob;
    IFT(m_dxcSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    IFT(pLib->CreateBlobFromBlob(pContent, offset, bitcodeLength,
                                 &pModuleBlob));
    std::swap(pModuleBlob, pContent);
  }

  WriteBlobToFile(pContent, StringRefUtf16(OutputFilename),
                  DXC_CP_UTF8); // TODO: Support DefaultTextCodePage
  printf("%Iu bytes written to %s\n", pContent->GetBufferSize(),
         OutputFilename.c_str());
  return true;
}

void DxaContext::ListParts() {
  CComPtr<IDxcBlobEncoding> pSource;
  ReadFileIntoBlob(m_dxcSupport, StringRefUtf16(InputFilename), &pSource);

  CComPtr<IDxcContainerReflection> pReflection;
  IFT(m_dxcSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  IFT(pReflection->Load(pSource));

  UINT32 partCount;
  IFT(pReflection->GetPartCount(&partCount));
  printf("Part count: %u\n", partCount);

  for (UINT32 i = 0; i < partCount; ++i) {
    UINT32 partKind;
    IFT(pReflection->GetPartKind(i, &partKind));
    // Part kind is typically four characters.
    char kindText[5];
    hlsl::PartKindToCharArray(partKind, kindText);

    CComPtr<IDxcBlob> partContent;
    IFT(pReflection->GetPartContent(i, &partContent));

    printf("#%u - %s (%u bytes)\n", i, kindText, (unsigned)partContent->GetBufferSize());
  }
}

void DxaContext::DumpRS() {
  const char *pName = "RTS0";
  const UINT32 matchName =
      ((UINT32)pName[0] | ((UINT32)pName[1] << 8) | ((UINT32)pName[2] << 16) |
       ((UINT32)pName[3] << 24));
  CComPtr<IDxcBlob> pContent;
  if (!ExtractPart(matchName, &pContent)) {
    printf("cannot find root signature part");
    return;
  }

  const void *serializedData = pContent->GetBufferPointer();
  uint32_t serializedSize = pContent->GetBufferSize();
  hlsl::RootSignatureHandle rootsig;
  rootsig.LoadSerialized(static_cast<const uint8_t *>(serializedData),
                         serializedSize);
  try {
    rootsig.Deserialize();
  } catch (const hlsl::Exception &e) {
    printf("fail to deserialize root sig %s", e.msg.c_str());
    return;
  }

  if (const hlsl::DxilVersionedRootSignatureDesc *pRS = rootsig.GetDesc()) {
    std::string str;
    llvm::raw_string_ostream os(str);
    hlsl::printRootSignature(*pRS, os);
    printf("%s", str.c_str());
  }
}

using namespace hlsl::options;

int __cdecl main(int argc, _In_reads_z_(argc) char **argv) {
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    return 1;
  llvm::sys::fs::AutoCleanupPerThreadFileSystem auto_cleanup_fs;
  if (FAILED(DxcInitThreadMalloc())) return 1;
  DxcSetThreadMallocToDefault();

  const char *pStage = "Operation";
  try {
    llvm::sys::fs::MSFileSystem *msfPtr;
    IFT(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    pStage = "Argument processing";

    // Parse command line options.
    cl::ParseCommandLineOptions(argc, argv, "dxil assembly\n");

    if (InputFilename == "" || Help) {
      cl::PrintHelpMessage();
      return 2;
    }


    DxcDllSupport dxcSupport;
    dxc::EnsureEnabled(dxcSupport);
    DxaContext context(dxcSupport);
    if (ListParts) {
      pStage = "Listing parts";
      context.ListParts();
    }
    else if (ListFiles) {
      pStage = "Listing files";
      context.ListFiles();
    }
    else if (!ExtractPart.empty()) {
      pStage = "Extracting part";
      if (!context.ExtractPart(ExtractPart.c_str())) {
        return 1;
      }
    }
    else if (!ExtractFile.empty()) {
      pStage = "Extracting files";
      if (!context.ExtractFile(ExtractFile.c_str())) {
        return 1;
      }
    } else if (DumpRootSig) {
      pStage = "Dump root sig";
      context.DumpRS();
    }
    else {
      pStage = "Assembling";
      context.Assemble();
    }
  } catch (const ::hlsl::Exception &hlslException) {
    try {
      const char *msg = hlslException.what();
      Unicode::acp_char printBuffer[128]; // printBuffer is safe to treat as
                                          // UTF-8 because we use ASCII only errors
                                          // only
      if (msg == nullptr || *msg == '\0') {
        sprintf_s(printBuffer, _countof(printBuffer),
                  "Assembly failed - error code 0x%08x.", hlslException.hr);
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