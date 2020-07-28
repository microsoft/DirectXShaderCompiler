///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcompilerobj.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler.                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "llvm/IR/LLVMContext.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxcutil.h"
#include "dxc/Support/dxcfilesystem.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/DXIL/DxilPDB.h"

#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/DxcLangExtensionsHelper.h"
#include "dxc/Support/HLSLOptions.h"
#ifdef _WIN32
#include "dxcetw.h"
#endif
#include "dxillib.h"
#include "dxcompileradapter.h"
#include <algorithm>
#include <cfloat>

// SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
#include "clang/SPIRV/EmitSpirvAction.h"
#endif
// SPIRV change ends

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

#define CP_UTF16 1200

using namespace llvm;
using namespace clang;
using namespace hlsl;
using std::string;

DEFINE_CROSS_PLATFORM_UUIDOF(IDxcLangExtensions)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcLangExtensions2)

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
                             _In_ llvm::Module *pModule,
                             _In_ llvm::Module *pDebugModule,
                             _In_ IDxcBlob *pShader, UINT32 Flags,
                             _In_ IDxcOperationResult **ppResult);

static bool ShouldPartBeIncludedInPDB(UINT32 FourCC) {
  switch (FourCC) {
  case hlsl::DFCC_ShaderDebugName:
  case hlsl::DFCC_ShaderHash:
    return true;
  }
  return false;
}

static HRESULT CreateContainerForPDB(IMalloc *pMalloc, IDxcBlob *pOldContainer, IDxcBlob *pDebugBlob, IDxcBlob **ppNewContaner) {
  // If the pContainer is not a valid container, give up.
  if (!hlsl::IsValidDxilContainer((hlsl::DxilContainerHeader *)pOldContainer->GetBufferPointer(), pOldContainer->GetBufferSize()))
    return E_FAIL;

  hlsl::DxilContainerHeader *DxilHeader = (hlsl::DxilContainerHeader *)pOldContainer->GetBufferPointer();
  hlsl::DxilProgramHeader *ProgramHeader = nullptr;

  struct Part {
    typedef std::function<HRESULT(IStream *)> WriteProc;
    UINT32 uFourCC = 0;
    UINT32 uSize = 0;
    WriteProc Writer;

    Part(UINT32 uFourCC, UINT32 uSize, WriteProc Writer) :
      uFourCC(uFourCC),
      uSize(uSize),
      Writer(Writer)
    {}
  };

  // Compute offset table.
  SmallVector<UINT32, 4> OffsetTable;
  SmallVector<Part, 4> PartWriters;
  UINT32 uTotalPartsSize = 0;
  for (unsigned i = 0; i < DxilHeader->PartCount; i++) {
    hlsl::DxilPartHeader *PartHeader = GetDxilContainerPart(DxilHeader, i);
    if (ShouldPartBeIncludedInPDB(PartHeader->PartFourCC)) {
      OffsetTable.push_back(uTotalPartsSize);
      uTotalPartsSize += PartHeader->PartSize + sizeof(*PartHeader);

      UINT32 uSize = PartHeader->PartSize;
      const void *pPartData = PartHeader+1;
      Part NewPart(
        PartHeader->PartFourCC,
        uSize,
        [pPartData, uSize](IStream *pStream) {
          ULONG uBytesWritten = 0;
          IFR(pStream->Write(pPartData, uSize, &uBytesWritten));
          return S_OK;
        }
      );
      PartWriters.push_back(NewPart);
    }

    // Could use any of these. We're mostly after the header version and all that.
    if (PartHeader->PartFourCC == hlsl::DFCC_DXIL ||
      PartHeader->PartFourCC == hlsl::DFCC_ShaderDebugInfoDXIL)
    {
      ProgramHeader = (hlsl::DxilProgramHeader *)(PartHeader+1);
    }
  }

  if (!ProgramHeader)
    return E_FAIL;

  {
    static auto AlignByDword = [](UINT32 uSize, UINT32 *pPaddingBytes) {
      UINT32 uRem = uSize % sizeof(UINT32);
      UINT32 uResult = (uSize/sizeof(UINT32) + (uRem ? 1 : 0)) * sizeof(UINT32);
      *pPaddingBytes = uRem ? (sizeof(UINT32)-uRem) : 0;
      return uResult;
    };

    UINT32 uPaddingSize = 0;
    UINT32 uPartSize = AlignByDword(sizeof(hlsl::DxilProgramHeader) + pDebugBlob->GetBufferSize(), &uPaddingSize);

    OffsetTable.push_back(uTotalPartsSize);
    uTotalPartsSize += uPartSize + sizeof(hlsl::DxilPartHeader);

    Part NewPart(
      hlsl::DFCC_ShaderDebugInfoDXIL,
      uPartSize,
      [uPartSize, ProgramHeader, pDebugBlob, uPaddingSize](IStream *pStream) {
        hlsl::DxilProgramHeader Header = *ProgramHeader;
        Header.BitcodeHeader.BitcodeSize = pDebugBlob->GetBufferSize();
        Header.BitcodeHeader.BitcodeOffset = sizeof(hlsl::DxilBitcodeHeader);
        Header.SizeInUint32 = uPartSize / sizeof(UINT32);

        ULONG uBytesWritten = 0;
        IFR(pStream->Write(&Header, sizeof(Header), &uBytesWritten));
        IFR(pStream->Write(pDebugBlob->GetBufferPointer(), pDebugBlob->GetBufferSize(), &uBytesWritten));
        if(uPaddingSize) {
          UINT32 uPadding = 0;
          assert(uPaddingSize <= sizeof(uPadding) && "Padding size calculation is wrong.");
          IFR(pStream->Write(&uPadding, uPaddingSize, &uBytesWritten));
        }
        return S_OK;
      }
    );
    PartWriters.push_back(NewPart);
  }

  // Offset the offset table by the offset table itself
  for (unsigned i = 0; i < OffsetTable.size(); i++)
    OffsetTable[i] += sizeof(hlsl::DxilContainerHeader) + OffsetTable.size() * sizeof(UINT32);

  // Create the new header
  hlsl::DxilContainerHeader NewDxilHeader = *DxilHeader;
  NewDxilHeader.PartCount = OffsetTable.size();
  NewDxilHeader.ContainerSizeInBytes =
    sizeof(NewDxilHeader) +
    OffsetTable.size() * sizeof(UINT32) +
    uTotalPartsSize;

  // Write it to the result stream
  ULONG uSizeWritten = 0;
  CComPtr<hlsl::AbstractMemoryStream> pStrippedContainerStream;
  IFR(hlsl::CreateMemoryStream(pMalloc, &pStrippedContainerStream));
  IFR(pStrippedContainerStream->Write(&NewDxilHeader, sizeof(NewDxilHeader), &uSizeWritten));

  // Write offset table
  IFR(pStrippedContainerStream->Write(OffsetTable.data(), OffsetTable.size() * sizeof(OffsetTable.data()[0]), &uSizeWritten));

  for (unsigned i = 0; i < PartWriters.size(); i++) {
    auto &Writer = PartWriters[i];
    hlsl::DxilPartHeader PartHeader = {};
    PartHeader.PartFourCC = Writer.uFourCC;
    PartHeader.PartSize = Writer.uSize;
    IFR(pStrippedContainerStream->Write(&PartHeader, sizeof(PartHeader), &uSizeWritten));
    IFR(Writer.Writer(pStrippedContainerStream));
  }

  IFR(pStrippedContainerStream.QueryInterface(ppNewContaner));

  return S_OK;
}

#ifdef _WIN32

#pragma fenv_access(on)

struct DefaultFPEnvScope
{
  // _controlfp_s is non-standard and <cfenv>.feholdexceptions doesn't work on windows...?
  unsigned int previousValue;
  DefaultFPEnvScope() {
    // _controlfp_s returns the value of the control word as it is after
    // the call, not before the call.  This is the proper way to get the
    // previous value.
    errno_t error = _controlfp_s(&previousValue, 0, 0);
    IFT(error == 0 ? S_OK : E_FAIL);
    // No exceptions, preserve denormals & round to nearest.
    unsigned int newValue;
    error = _controlfp_s(&newValue, _MCW_EM | _DN_SAVE | _RC_NEAR,
                                    _MCW_EM | _MCW_DN  | _MCW_RC);
    IFT(error == 0 ? S_OK : E_FAIL);
  }
  ~DefaultFPEnvScope() {
    _clearfp();
    unsigned int newValue;
    errno_t error = _controlfp_s(&newValue, previousValue, _MCW_EM | _MCW_DN | _MCW_RC);
    // During cleanup we can't throw as we might already be handling another one.
    DXASSERT_LOCALVAR(error, error == 0, "Failed to restore floating-point environment.");
  }
};

#else   // _WIN32

struct DefaultFPEnvScope {
  DefaultFPEnvScope() {} // Dummy ctor to avoid unused local warning
};

#endif  // _WIN32

class HLSLExtensionsCodegenHelperImpl : public HLSLExtensionsCodegenHelper {
private:
  CompilerInstance &m_CI;
  DxcLangExtensionsHelper &m_langExtensionsHelper;
  std::string m_rootSigDefine;

  // The metadata format is a root node that has pointers to metadata
  // nodes for each define. The metatdata node for a define is a pair
  // of (name, value) metadata strings.
  //
  // Example:
  // !hlsl.semdefs = {!0, !1}
  // !0 = !{!"FOO", !"BAR"}
  // !1 = !{!"BOO", !"HOO"}
  void WriteSemanticDefines(llvm::Module *M, const ParsedSemanticDefineList &defines) {
    // Create all metadata nodes for each define. Each node is a (name, value) pair.
    std::vector<MDNode *> mdNodes;
    const std::string enableStr("_ENABLE_");
    const std::string disableStr("_DISABLE_");
    const std::string selectStr("_SELECT_");

    auto &optToggles = m_CI.getCodeGenOpts().HLSLOptimizationToggles;
    auto &optSelects = m_CI.getCodeGenOpts().HLSLOptimizationSelects;

    const llvm::SmallVector<std::string, 2> &semDefPrefixes =
                             m_langExtensionsHelper.GetSemanticDefines();

    // Add semantic defines to mdNodes and also to codeGenOpts
    for (const ParsedSemanticDefine &define : defines) {
      MDString *name  = MDString::get(M->getContext(), define.Name);
      MDString *value = MDString::get(M->getContext(), define.Value);
      mdNodes.push_back(MDNode::get(M->getContext(), { name, value }));

      // Find index for end of matching semantic define prefix
      size_t prefixPos = 0;
      for (auto prefix : semDefPrefixes) {
        if (IsMacroMatch(define.Name, prefix)) {
          prefixPos = prefix.length() - 1;
          break;
        }
      }

      // Add semantic defines to option flag equivalents
      if (!define.Name.compare(prefixPos, enableStr.length(), enableStr))
        optToggles[StringRef(define.Name.substr(prefixPos + enableStr.length())).lower()] = true;
      else if (!define.Name.compare(prefixPos, disableStr.length(), disableStr))
        optToggles[StringRef(define.Name.substr(prefixPos + disableStr.length())).lower()] = false;
      else if (!define.Name.compare(prefixPos, selectStr.length(), selectStr))
        optSelects[StringRef(define.Name.substr(prefixPos + selectStr.length())).lower()] = define.Value;
    }

    // Add root node with pointers to all define metadata nodes.
    NamedMDNode *Root = M->getOrInsertNamedMetadata(m_langExtensionsHelper.GetSemanticDefineMetadataName());
    for (MDNode *node : mdNodes)
      Root->addOperand(node);
  }

  SemanticDefineErrorList GetValidatedSemanticDefines(const ParsedSemanticDefineList &defines, ParsedSemanticDefineList &validated, SemanticDefineErrorList &errors) {
    for (const ParsedSemanticDefine &define : defines) {
      DxcLangExtensionsHelper::SemanticDefineValidationResult result = m_langExtensionsHelper.ValidateSemanticDefine(define.Name, define.Value);
        if (result.HasError())
          errors.emplace_back(SemanticDefineError(define.Location, SemanticDefineError::Level::Error, result.Error));
        if (result.HasWarning())
          errors.emplace_back(SemanticDefineError(define.Location, SemanticDefineError::Level::Warning, result.Warning));
        if (!result.HasError())
          validated.emplace_back(define);
    }

    return errors;
  }

public:
  HLSLExtensionsCodegenHelperImpl(CompilerInstance &CI, DxcLangExtensionsHelper &langExtensionsHelper, StringRef rootSigDefine)
  : m_CI(CI), m_langExtensionsHelper(langExtensionsHelper)
  , m_rootSigDefine(rootSigDefine)
  {}

  // Write semantic defines as metadata in the module.
  virtual std::vector<SemanticDefineError> WriteSemanticDefines(llvm::Module *M) override {
    // Grab the semantic defines seen by the parser.
    ParsedSemanticDefineList defines =
      CollectSemanticDefinesParsedByCompiler(m_CI, &m_langExtensionsHelper);

    // Nothing to do if we have no defines.
    SemanticDefineErrorList errors;
    if (!defines.size())
      return errors;

    ParsedSemanticDefineList validated;
    GetValidatedSemanticDefines(defines, validated, errors);
    WriteSemanticDefines(M, validated);
    return errors;
  }

  virtual std::string GetIntrinsicName(UINT opcode) override {
    return m_langExtensionsHelper.GetIntrinsicName(opcode);
  }
  
  virtual bool GetDxilOpcode(UINT opcode, OP::OpCode &dxilOpcode) override {
    UINT dop = static_cast<UINT>(OP::OpCode::NumOpCodes);
    if (m_langExtensionsHelper.GetDxilOpCode(opcode, dop)) {
      if (dop < static_cast<UINT>(OP::OpCode::NumOpCodes)) {
        dxilOpcode = static_cast<OP::OpCode>(dop);
        return true;
      }
    }
    return false;
  }

  virtual HLSLExtensionsCodegenHelper::CustomRootSignature::Status GetCustomRootSignature(CustomRootSignature *out) override {
    // Find macro definition in preprocessor.
    Preprocessor &pp = m_CI.getPreprocessor();
    MacroInfo *macro = MacroExpander::FindMacroInfo(pp, m_rootSigDefine);
    if (!macro)
      return CustomRootSignature::NOT_FOUND;

    // Combine tokens into single string
    MacroExpander expander(pp, MacroExpander::STRIP_QUOTES);
    if (!expander.ExpandMacro(macro, &out->RootSignature))
      return CustomRootSignature::NOT_FOUND;

    // Record source location of root signature macro.
    out->EncodedSourceLocation = macro->getDefinitionLoc().getRawEncoding();

    return CustomRootSignature::FOUND;
  }
};

static void CreateDefineStrings(
    _In_count_(defineCount) const DxcDefine *pDefines,
    UINT defineCount,
    std::vector<std::string> &defines) {
  // Not very efficient but also not very important.
  for (UINT32 i = 0; i < defineCount; i++) {
    CW2A utf8Name(pDefines[i].Name, CP_UTF8);
    CW2A utf8Value(pDefines[i].Value, CP_UTF8);
    std::string val(utf8Name.m_psz);
    val += "=";
    val += (pDefines[i].Value) ? utf8Value.m_psz : "1";
    defines.push_back(val);
  }
}

class DxcCompiler : public IDxcCompiler3,
                    public IDxcLangExtensions2,
                    public IDxcContainerEvent,
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
                    public IDxcVersionInfo2
#else
                    public IDxcVersionInfo
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  DxcLangExtensionsHelper m_langExtensionsHelper;
  CComPtr<IDxcContainerEventsHandler> m_pDxcContainerEventsHandler;
  DxcCompilerAdapter m_DxcCompilerAdapter;

public:
  DxcCompiler(IMalloc *pMalloc) : m_dwRef(0), m_pMalloc(pMalloc), m_DxcCompilerAdapter(this, pMalloc) {}
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcCompiler)
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langExtensionsHelper)

  HRESULT STDMETHODCALLTYPE RegisterDxilContainerEventHandler(IDxcContainerEventsHandler *pHandler, UINT64 *pCookie) override {
    DXASSERT(m_pDxcContainerEventsHandler == nullptr, "else events handler is already registered");
    *pCookie = 1; // Only one EventsHandler supported 
    m_pDxcContainerEventsHandler = pHandler;
    return S_OK;
  };
  HRESULT STDMETHODCALLTYPE UnRegisterDxilContainerEventHandler(UINT64 cookie) override {
    DXASSERT(m_pDxcContainerEventsHandler != nullptr, "else unregister should not have been called");
    m_pDxcContainerEventsHandler.Release();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    HRESULT hr = DoBasicQueryInterface<
      IDxcCompiler3,
      IDxcLangExtensions,
      IDxcLangExtensions2,
      IDxcContainerEvent,
      IDxcVersionInfo
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
      ,IDxcVersionInfo2
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
     >
     (this, iid, ppvObject);
    if (FAILED(hr)) {
      return DoBasicQueryInterface<IDxcCompiler, IDxcCompiler2>(&m_DxcCompilerAdapter, iid, ppvObject);
    }
    return hr;
  }

  // Compile a single entry point to the target shader model with debug information.
  HRESULT STDMETHODCALLTYPE Compile(
    _In_ const DxcBuffer *pSource,                // Source text to compile
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _In_ REFIID riid, _Out_ LPVOID *ppResult      // IDxcResult: status, buffer, and errors
  ) override {
    if (pSource == nullptr ||
        (argCount > 0 && pArguments == nullptr))
      return E_INVALIDARG;
    if (!(IsEqualIID(riid, __uuidof(IDxcResult)) ||
          IsEqualIID(riid, __uuidof(IDxcOperationResult))))
      return E_INVALIDARG;

    *ppResult = nullptr;

    HRESULT hr = S_OK;
    CComPtr<IDxcBlobUtf8> utf8Source;
    CComPtr<AbstractMemoryStream> pOutputStream;
    CComPtr<IDxcOperationResult> pDxcOperationResult;
    bool bCompileStarted = false;
    bool bPreprocessStarted = false;
    DxilShaderHash ShaderHashContent;
    DxcThreadMalloc TM(m_pMalloc);

    try {
      DefaultFPEnvScope fpEnvScope;

      IFT(CreateMemoryStream(m_pMalloc, &pOutputStream));

      // Parse command-line options into DxcOpts
      int argCountInt;
      IFT(UIntToInt(argCount, &argCountInt));
      hlsl::options::MainArgs mainArgs(argCountInt, pArguments, 0);
      hlsl::options::DxcOpts opts;
      std::string warnings;
      raw_string_ostream w(warnings);
      {
        bool finished = false;
        CComPtr<AbstractMemoryStream> pOptionErrorStream;
        IFT(CreateMemoryStream(m_pMalloc, &pOptionErrorStream));
        dxcutil::ReadOptsAndValidate(mainArgs, opts, pOptionErrorStream, &pDxcOperationResult, finished);
        if (finished) {
          IFT(pDxcOperationResult->QueryInterface(riid, ppResult));
          hr = S_OK;
          goto Cleanup;
        }
        if (pOptionErrorStream->GetPtrSize() > 0) {
          w << StringRef((const char*)pOptionErrorStream->GetPtr(), (size_t)pOptionErrorStream->GetPtrSize());
        }
      }

      bool isPreprocessing = !opts.Preprocess.empty();
      if (isPreprocessing) {
        DxcEtw_DXCompilerPreprocess_Start();
        bPreprocessStarted = true;
      } else {
        DxcEtw_DXCompilerCompile_Start();
        bCompileStarted = true;
      }

      CComPtr<DxcResult> pResult = DxcResult::Alloc(m_pMalloc);
      IFT(pResult->SetEncoding(opts.DefaultTextCodePage));
      DxcOutputObject primaryOutput;

      // Formerly API values.
      const char *pUtf8SourceName = opts.InputFile.empty() ? "hlsl.hlsl" : opts.InputFile.data();
      CA2W pUtf16SourceName(pUtf8SourceName, CP_UTF8);
      const char *pUtf8EntryPoint = opts.EntryPoint.empty() ? "main" : opts.EntryPoint.data();
      const char *pUtf8OutputName = isPreprocessing
                                    ? opts.Preprocess.data()
                                    : opts.OutputObject.empty()
                                      ? "" : opts.OutputObject.data();
      CA2W pUtf16OutputName(isPreprocessing ?
                              opts.Preprocess.data() : pUtf8OutputName,
                            CP_UTF8);
      LPCWSTR pObjectName = (!isPreprocessing && opts.OutputObject.empty()) ?
                            nullptr : pUtf16OutputName.m_psz;
      IFT(primaryOutput.SetName(pObjectName));

      // Wrap source in blob
      CComPtr<IDxcBlobEncoding> pSourceEncoding;
      IFT(hlsl::DxcCreateBlob(pSource->Ptr, pSource->Size,
        true, false, pSource->Encoding != 0, pSource->Encoding,
        nullptr, &pSourceEncoding));

 #ifdef ENABLE_SPIRV_CODEGEN
      // We want to embed the preprocessed source code in the final SPIR-V if
      // debug information is enabled. Therefore, we invoke Preprocess() here
      // first for such case. Then we invoke the compilation process over the
      // preprocessed source code, so that line numbers are consistent with the
      // embedded source code.
      if (!isPreprocessing && opts.GenSPIRV && opts.DebugInfo) {
        CComPtr<IDxcResult> pSrcCodeResult;
        std::vector<LPCWSTR> PreprocessArgs;
        PreprocessArgs.reserve(argCount + 1);
        PreprocessArgs.assign(pArguments, pArguments + argCount);
        PreprocessArgs.push_back(L"-P");
        PreprocessArgs.push_back(L"preprocessed.hlsl");
        IFT(Compile(pSource, PreprocessArgs.data(), PreprocessArgs.size(), pIncludeHandler, IID_PPV_ARGS(&pSrcCodeResult)));
        HRESULT status;
        IFT(pSrcCodeResult->GetStatus(&status));
        if (SUCCEEDED(status)) {
          pSourceEncoding.Release();
          IFT(pSrcCodeResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&pSourceEncoding), nullptr));
        }
      }
#endif // ENABLE_SPIRV_CODEGEN

      // Convert source code encoding
      IFC(hlsl::DxcGetBlobAsUtf8(pSourceEncoding, m_pMalloc, &utf8Source));

      CComPtr<IDxcBlob> pOutputBlob;
      dxcutil::DxcArgsFileSystem *msfPtr =
        dxcutil::CreateDxcArgsFileSystem(utf8Source, pUtf16SourceName.m_psz, pIncludeHandler);
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      IFT(pOutputStream.QueryInterface(&pOutputBlob));

      primaryOutput.kind = DXC_OUT_OBJECT;
      if (opts.AstDump || opts.OptDump)
        primaryOutput.kind = DXC_OUT_TEXT;
      else if (isPreprocessing)
        primaryOutput.kind = DXC_OUT_HLSL;

      IFT(pResult->SetOutputName(DXC_OUT_REFLECTION, opts.OutputReflectionFile));
      IFT(pResult->SetOutputName(DXC_OUT_SHADER_HASH, opts.OutputShaderHashFile));
      IFT(pResult->SetOutputName(DXC_OUT_ERRORS, opts.OutputWarningsFile));
      IFT(pResult->SetOutputName(DXC_OUT_ROOT_SIGNATURE, opts.OutputRootSigFile));

      if (opts.DisplayIncludeProcess)
        msfPtr->EnableDisplayIncludeProcess();

      IFT(msfPtr->RegisterOutputStream(L"output.bc", pOutputStream));
      IFT(msfPtr->CreateStdStreams(m_pMalloc));

      StringRef Data(utf8Source->GetStringPointer(),
                     utf8Source->GetStringLength());

      // Not very efficient but also not very important.
      std::vector<std::string> defines;
      CreateDefineStrings(opts.Defines.data(), opts.Defines.size(), defines);

      // Setup a compiler instance.
      raw_stream_ostream outStream(pOutputStream.p);
      llvm::LLVMContext llvmContext; // LLVMContext should outlive CompilerInstance
      CompilerInstance compiler;
      std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
          llvm::make_unique<TextDiagnosticPrinter>(w, &compiler.getDiagnosticOpts());
      SetupCompilerForCompile(compiler, &m_langExtensionsHelper, pUtf8SourceName, diagPrinter.get(), defines, opts, pArguments, argCount);
      msfPtr->SetupForCompilerInstance(compiler);

      // The clang entry point (cc1_main) would now create a compiler invocation
      // from arguments, but depending on the Preprocess option, we either compile
      // to LLVM bitcode and then package that into a DXBC blob, or preprocess to
      // HLSL text.
      //
      // With the compiler invocation built from command line arguments, the
      // next step is to call ExecuteCompilerInvocation, which creates a
      // FrontendAction* of EmitBCAction, which is a CodeGenAction, which is an
      // ASTFrontendAction. That sets up a BackendConsumer as the ASTConsumer.
      compiler.getFrontendOpts().OutputFile = "output.bc";
      compiler.WriteDefaultOutputDirectly = true;
      compiler.setOutStream(&outStream);

      unsigned rootSigMajor = 0;
      unsigned rootSigMinor = 0;
      // NOTE: this calls the validation component from dxil.dll; the built-in
      // validator can be used as a fallback.
      bool produceFullContainer = false;
      bool needsValidation = false;
      bool validateRootSigContainer = false;

      if (isPreprocessing) {
        // These settings are back-compatible with fxc.
        clang::PreprocessorOutputOptions &PPOutOpts =
          compiler.getPreprocessorOutputOpts();
        PPOutOpts.ShowCPP = 1;            // Print normal preprocessed output.
        PPOutOpts.ShowComments = 0;       // Show comments.
        PPOutOpts.ShowLineMarkers = 1;    // Show \#line markers.
        PPOutOpts.UseLineDirectives = 1;  // Use \#line instead of GCC-style \# N.
        PPOutOpts.ShowMacroComments = 0;  // Show comments, even in macros.
        PPOutOpts.ShowMacros = 0;         // Print macro definitions.
        PPOutOpts.RewriteIncludes = 0;    // Preprocess include directives only.

        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        clang::PrintPreprocessedAction action;
        if (action.BeginSourceFile(compiler, file)) {
          action.Execute();
          action.EndSourceFile();
        }
        outStream.flush();
      } else {
        compiler.getLangOpts().HLSLEntryFunction =
          compiler.getCodeGenOpts().HLSLEntryFunction = pUtf8EntryPoint;
        compiler.getLangOpts().HLSLProfile =
          compiler.getCodeGenOpts().HLSLProfile = opts.TargetProfile;

        if (compiler.getCodeGenOpts().HLSLProfile == "rootsig_1_1") {
          rootSigMajor = 1;
          rootSigMinor = 1;
        } else if (compiler.getCodeGenOpts().HLSLProfile == "rootsig_1_0") {
          rootSigMajor = 1;
          rootSigMinor = 0;
        }
        compiler.getLangOpts().IsHLSLLibrary = opts.IsLibraryProfile();

        // Clear entry function if library target
        if (compiler.getLangOpts().IsHLSLLibrary)
          compiler.getLangOpts().HLSLEntryFunction =
            compiler.getCodeGenOpts().HLSLEntryFunction = "";

        // NOTE: this calls the validation component from dxil.dll; the built-in
        // validator can be used as a fallback.
        produceFullContainer = !opts.CodeGenHighLevel && !opts.AstDump && !opts.OptDump && rootSigMajor == 0;
        needsValidation = produceFullContainer && !opts.DisableValidation;

        if (compiler.getCodeGenOpts().HLSLProfile == "lib_6_x") {
          // Currently do not support stripping reflection from offline linking target.
          opts.KeepReflectionInDxil = true;
        }

        if (opts.ValVerMajor != UINT_MAX) {
          // user-specified validator version override
          compiler.getCodeGenOpts().HLSLValidatorMajorVer = opts.ValVerMajor;
          compiler.getCodeGenOpts().HLSLValidatorMinorVer = opts.ValVerMinor;
        } else {
          // Version from dxil.dll, or internal validator if unavailable
          dxcutil::GetValidatorVersion(&compiler.getCodeGenOpts().HLSLValidatorMajorVer,
                                      &compiler.getCodeGenOpts().HLSLValidatorMinorVer);
        }

        // Root signature-only container validation is only supported on 1.5 and above.
        validateRootSigContainer = DXIL::CompareVersions(
          compiler.getCodeGenOpts().HLSLValidatorMajorVer,
          compiler.getCodeGenOpts().HLSLValidatorMinorVer,
          1, 5) >= 0;
      }

      if (opts.AstDump) {
        clang::ASTDumpAction dumpAction;
        // Consider - ASTDumpFilter, ASTDumpLookups
        compiler.getFrontendOpts().ASTDumpDecls = true;
        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        dumpAction.BeginSourceFile(compiler, file);
        dumpAction.Execute();
        dumpAction.EndSourceFile();
        outStream.flush();
      }
      else if (opts.OptDump) {
        EmitOptDumpAction action(&llvmContext);
        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        action.BeginSourceFile(compiler, file);
        action.Execute();
        action.EndSourceFile();
        outStream.flush();
      }
      else if (rootSigMajor) {
        HLSLRootSignatureAction action(
            compiler.getCodeGenOpts().HLSLEntryFunction, rootSigMajor,
            rootSigMinor);
        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        action.BeginSourceFile(compiler, file);
        action.Execute();
        action.EndSourceFile();
        outStream.flush();
        // Don't do work to put in a container if an error has occurred
        bool compileOK = !compiler.getDiagnostics().hasErrorOccurred();
        if (compileOK) {
          auto rootSigHandle = action.takeRootSigHandle();

          CComPtr<AbstractMemoryStream> pContainerStream;
          IFT(CreateMemoryStream(m_pMalloc, &pContainerStream));
          SerializeDxilContainerForRootSignature(rootSigHandle.get(),
                                                 pContainerStream);

          pOutputBlob.Release();
          IFT(pContainerStream.QueryInterface(&pOutputBlob));
          if (validateRootSigContainer && !opts.DisableValidation) {
            CComPtr<IDxcBlobEncoding> pValErrors;
            // Validation failure communicated through diagnostic error
            dxcutil::ValidateRootSignatureInContainer(
              pOutputBlob, &compiler.getDiagnostics());
          }
        }
      }
      // SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
      else if (!isPreprocessing && opts.GenSPIRV) {
        // Since SpirvOptions is passed to the SPIR-V CodeGen as a whole
        // structure, we need to copy a few non-spirv-specific options into the
        // structure.
        opts.SpirvOptions.enable16BitTypes = opts.Enable16BitTypes;
        opts.SpirvOptions.codeGenHighLevel = opts.CodeGenHighLevel;
        opts.SpirvOptions.defaultRowMajor = opts.DefaultRowMajor;
        opts.SpirvOptions.disableValidation = opts.DisableValidation;
        // Store a string representation of command line options.
        if (opts.DebugInfo)
          for (auto opt : mainArgs.getArrayRef())
            opts.SpirvOptions.clOptions += " " + std::string(opt);

        compiler.getCodeGenOpts().SpirvOptions = opts.SpirvOptions;
        clang::EmitSpirvAction action;
        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        action.BeginSourceFile(compiler, file);
        action.Execute();
        action.EndSourceFile();
        outStream.flush();
      }
#endif
      // SPIRV change ends
      else if (!isPreprocessing) {
        EmitBCAction action(&llvmContext);
        FrontendInputFile file(pUtf8SourceName, IK_HLSL);
        bool compileOK;
        if (action.BeginSourceFile(compiler, file)) {
          action.Execute();
          action.EndSourceFile();
          compileOK = !compiler.getDiagnostics().hasErrorOccurred();
        }
        else {
          compileOK = false;
        }
        outStream.flush();

        SerializeDxilFlags SerializeFlags = SerializeDxilFlags::None;
        if (opts.EmbedPDBName()) {
          SerializeFlags |= SerializeDxilFlags::IncludeDebugNamePart;
        }
        // If -Qembed_debug specified, embed the debug info.
        // Or, if there is no output pointer for the debug blob (such as when called by Compile()),
        // embed the debug info and emit a note.
        if (opts.EmbedDebugInfo()) {
          SerializeFlags |= SerializeDxilFlags::IncludeDebugInfoPart;
        }
        if (opts.DebugNameForSource) {
          // Implies name part
          SerializeFlags |= SerializeDxilFlags::IncludeDebugNamePart;
          SerializeFlags |= SerializeDxilFlags::DebugNameDependOnSource;
        } else if (opts.DebugNameForBinary) {
          // Implies name part
          SerializeFlags |= SerializeDxilFlags::IncludeDebugNamePart;
        }
        if (!opts.KeepReflectionInDxil) {
          SerializeFlags |= SerializeDxilFlags::StripReflectionFromDxilPart;
        }
        if (!opts.StripReflection) {
          SerializeFlags |= SerializeDxilFlags::IncludeReflectionPart;
        }
        if (opts.StripRootSignature) {
          SerializeFlags |= SerializeDxilFlags::StripRootSignature;
        }

        // Don't do work to put in a container if an error has occurred
        // Do not create a container when there is only a a high-level representation in the module.
        if (compileOK && !opts.CodeGenHighLevel) {
          HRESULT valHR = S_OK;
          CComPtr<AbstractMemoryStream> pReflectionStream;
          CComPtr<AbstractMemoryStream> pRootSigStream;
          IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pReflectionStream));
          IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pRootSigStream));

          dxcutil::AssembleInputs inputs(
                action.takeModule(), pOutputBlob, m_pMalloc, SerializeFlags,
                pOutputStream, opts.IsDebugInfoEnabled(),
                opts.GetPDBName(), &compiler.getDiagnostics(),
                &ShaderHashContent, pReflectionStream, pRootSigStream);
          if (needsValidation) {
            valHR = dxcutil::ValidateAndAssembleToContainer(inputs);
          } else {
            dxcutil::AssembleToContainer(inputs);
          }

          // Callback after valid DXIL is produced
          if (SUCCEEDED(valHR)) {
            CComPtr<IDxcBlob> pTargetBlob;
            if (m_pDxcContainerEventsHandler != nullptr) {
              HRESULT hr = m_pDxcContainerEventsHandler->OnDxilContainerBuilt(pOutputBlob, &pTargetBlob);
              if (SUCCEEDED(hr) && pTargetBlob != nullptr) {
                std::swap(pOutputBlob, pTargetBlob);
              }
            }

            if (pOutputBlob && produceFullContainer && (SerializeFlags & SerializeDxilFlags::IncludeDebugNamePart) != 0) {
              const DxilContainerHeader *pContainer = reinterpret_cast<DxilContainerHeader *>(pOutputBlob->GetBufferPointer());
              DXASSERT(IsValidDxilContainer(pContainer, pOutputBlob->GetBufferSize()), "else invalid container generated");
              auto it = std::find_if(begin(pContainer), end(pContainer), DxilPartIsType(DFCC_ShaderDebugName));
              if (it != end(pContainer)) {
                const char *pDebugName;
                if (GetDxilShaderDebugName(*it, &pDebugName, nullptr) && pDebugName && *pDebugName) {
                  IFT(pResult->SetOutputName(DXC_OUT_PDB, pDebugName));
                }
              }
            }

            if (pReflectionStream && pReflectionStream->GetPtrSize()) {
              CComPtr<IDxcBlob> pReflection;
              IFT(pReflectionStream->QueryInterface(&pReflection));
              IFT(pResult->SetOutputObject(DXC_OUT_REFLECTION, pReflection));
            }
            if (pRootSigStream && pRootSigStream->GetPtrSize()) {
              CComPtr<IDxcBlob> pRootSignature;
              IFT(pRootSigStream->QueryInterface(&pRootSignature));
              if (validateRootSigContainer && needsValidation) {
                CComPtr<IDxcBlobEncoding> pValErrors;
                // Validation failure communicated through diagnostic error
                dxcutil::ValidateRootSignatureInContainer(pRootSignature, &compiler.getDiagnostics());
              }
              IFT(pResult->SetOutputObject(DXC_OUT_ROOT_SIGNATURE, pRootSignature));
            }
            CComPtr<IDxcBlob> pHashBlob;
            IFT(hlsl::DxcCreateBlobOnHeapCopy(&ShaderHashContent, (UINT32)sizeof(ShaderHashContent), &pHashBlob));
            IFT(pResult->SetOutputObject(DXC_OUT_SHADER_HASH, pHashBlob));
          } // SUCCEEDED(valHR)
        } // compileOK && !opts.CodeGenHighLevel
      }

      // Add std err to warnings.
      msfPtr->WriteStdErrToStream(w);
      CComPtr<IStream> pErrorStream;
      msfPtr->GetStdOutpuHandleStream(&pErrorStream);
      CComPtr<IDxcBlob> pErrorBlob;
      IFT(pErrorStream.QueryInterface(&pErrorBlob));
      if (IsBlobNullOrEmpty(pErrorBlob)) {
        IFT(pResult->SetOutputString(DXC_OUT_ERRORS, warnings.c_str(), warnings.size()));
      } else {
        IFT(pResult->SetOutputObject(DXC_OUT_ERRORS, pErrorBlob));
      }

      bool hasErrorOccurred = compiler.getDiagnostics().hasErrorOccurred();

      bool writePDB = opts.IsDebugInfoEnabled() && produceFullContainer;

      // SPIRV change starts
#if defined(ENABLE_SPIRV_CODEGEN)
      writePDB &= !opts.GenSPIRV;
#endif
      // SPIRV change ends

      if (!hasErrorOccurred && writePDB) {
        CComPtr<IDxcBlob> pDebugBlob;
        IFT(pOutputStream.QueryInterface(&pDebugBlob));
        CComPtr<IDxcBlob> pStrippedContainer;
        IFT(CreateContainerForPDB(m_pMalloc, pOutputBlob, pDebugBlob, &pStrippedContainer));
        pDebugBlob.Release();
        IFT(hlsl::pdb::WriteDxilPDB(m_pMalloc, pStrippedContainer, ShaderHashContent.Digest, &pDebugBlob));
        IFT(pResult->SetOutputObject(DXC_OUT_PDB, pDebugBlob));
      }

      IFT(primaryOutput.SetObject(pOutputBlob, opts.DefaultTextCodePage));
      IFT(pResult->SetOutput(primaryOutput));
      IFT(pResult->SetStatusAndPrimaryResult(hasErrorOccurred ? E_FAIL : S_OK, primaryOutput.kind));
      IFT(pResult->QueryInterface(riid, ppResult));

      hr = S_OK;
    } catch (std::bad_alloc &) {
      hr = E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      _Analysis_assume_(DXC_FAILED(e.hr));
      CComPtr<IDxcResult> pResult;
      hr = e.hr;
      if (SUCCEEDED(DxcResult::Create(e.hr, DXC_OUT_NONE, {
              DxcOutputObject::ErrorOutput(CP_UTF8,
                e.msg.c_str(), e.msg.size())
            }, &pResult)) &&
          SUCCEEDED(pResult->QueryInterface(riid, ppResult))) {
        hr = S_OK;
      }
    } catch (...) {
      hr = E_FAIL;
    }
  Cleanup:
    if (bPreprocessStarted) {
      DxcEtw_DXCompilerPreprocess_Stop(hr);
    }
    if (bCompileStarted) {
      DxcEtw_DXCompilerCompile_Stop(hr);
    }
    return hr;
  }

  // Disassemble a program.
  virtual HRESULT STDMETHODCALLTYPE Disassemble(
    _In_ const DxcBuffer *pObject,                // Program to disassemble: dxil container or bitcode.
    _In_ REFIID riid, _Out_ LPVOID *ppResult      // IDxcResult: status, disassembly text, and errors
    ) override {
    if (pObject == nullptr || ppResult == nullptr)
      return E_INVALIDARG;
    if (!(IsEqualIID(riid, __uuidof(IDxcResult)) ||
          IsEqualIID(riid, __uuidof(IDxcOperationResult))))
      return E_INVALIDARG;

    *ppResult = nullptr;
    CComPtr<IDxcResult> pResult;

    HRESULT hr = S_OK;
    DxcEtw_DXCompilerDisassemble_Start();
    DxcThreadMalloc TM(m_pMalloc); 
    try {
      DefaultFPEnvScope fpEnvScope;

      ::llvm::sys::fs::MSFileSystem *msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      std::string StreamStr;
      raw_string_ostream Stream(StreamStr);

      CComPtr<IDxcBlobEncoding> pProgram;
      IFT(hlsl::DxcCreateBlob(pObject->Ptr, pObject->Size, true, false, false, 0, nullptr, &pProgram))
      IFC(dxcutil::Disassemble(pProgram, Stream));

      IFT(DxcResult::Create(S_OK, DXC_OUT_DISASSEMBLY, {
          DxcOutputObject::StringOutput(DXC_OUT_DISASSEMBLY,
            CP_UTF8, StreamStr.c_str(), StreamStr.size(), DxcOutNoName)
        }, &pResult));
      IFT(pResult->QueryInterface(riid, ppResult));

      return S_OK;
    } catch (std::bad_alloc &) {
      hr = E_OUTOFMEMORY;
    } catch (hlsl::Exception &e) {
      _Analysis_assume_(DXC_FAILED(e.hr));
      hr = e.hr;
      if (SUCCEEDED(DxcResult::Create(e.hr, DXC_OUT_NONE, {
              DxcOutputObject::ErrorOutput(CP_UTF8,
                e.msg.c_str(), e.msg.size())
            }, &pResult)) &&
          SUCCEEDED(pResult->QueryInterface(riid, ppResult))) {
        hr = S_OK;
      }
    } catch (...) {
      hr = E_FAIL;
    }
  Cleanup:
    DxcEtw_DXCompilerDisassemble_Stop(hr);
    return hr;
  }

  void SetupCompilerForCompile(CompilerInstance &compiler,
                               _In_ DxcLangExtensionsHelper *helper,
                               _In_ LPCSTR pMainFile, _In_ TextDiagnosticPrinter *diagPrinter,
                               _In_ std::vector<std::string>& defines,
                               _In_ hlsl::options::DxcOpts &Opts,
                               _In_count_(argCount) LPCWSTR *pArguments,
                               _In_ UINT32 argCount) {
    // Setup a compiler instance.
    std::shared_ptr<TargetOptions> targetOptions(new TargetOptions);
    targetOptions->Triple = "dxil-ms-dx";
    if (helper) {
      targetOptions->Triple = helper->GetTargetTriple();
    }
    targetOptions->DescriptionString = Opts.Enable16BitTypes
      ? hlsl::DXIL::kNewLayoutString
      : hlsl::DXIL::kLegacyLayoutString;
    compiler.HlslLangExtensions = helper;
    compiler.getDiagnosticOpts().ShowOptionNames = Opts.ShowOptionNames ? 1 : 0;
    compiler.getDiagnosticOpts().Warnings = std::move(Opts.Warnings);
    compiler.createDiagnostics(diagPrinter, false);
    // don't output warning to stderr/file if "/no-warnings" is present.
    compiler.getDiagnostics().setIgnoreAllWarnings(!Opts.OutputWarnings);
    compiler.createFileManager();
    compiler.createSourceManager(compiler.getFileManager());
    compiler.setTarget(
        TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), targetOptions));
    if (Opts.EnableDX9CompatMode) {
      auto const ID = compiler.getDiagnostics().getCustomDiagID(clang::DiagnosticsEngine::Warning, "/Gec flag is a deprecated functionality.");
      compiler.getDiagnostics().Report(ID);
    }

    compiler.getFrontendOpts().Inputs.push_back(FrontendInputFile(pMainFile, IK_HLSL));
    // Setup debug information.
    if (Opts.IsDebugInfoEnabled()) {
      CodeGenOptions &CGOpts = compiler.getCodeGenOpts();
      CGOpts.setDebugInfo(CodeGenOptions::FullDebugInfo);
      CGOpts.DebugColumnInfo = 1;
      CGOpts.DwarfVersion = 4; // Latest version.
      // TODO: consider
      // DebugPass, DebugCompilationDir, DwarfDebugFlags, SplitDwarfFile
    }

    clang::PreprocessorOptions &PPOpts(compiler.getPreprocessorOpts());
    for (size_t i = 0; i < defines.size(); ++i) {
      PPOpts.addMacroDef(defines[i]);
    }

    PPOpts.IgnoreLineDirectives = Opts.IgnoreLineDirectives;
    // fxc compatibility: pre-expand operands before performing token-pasting
    PPOpts.ExpandTokPastingArg = Opts.LegacyMacroExpansion;

    // Pick additional arguments.
    clang::HeaderSearchOptions &HSOpts = compiler.getHeaderSearchOpts();
    HSOpts.UseBuiltinIncludes = 0;
    // Consider: should we force-include '.' if the source file is relative?
    for (const llvm::opt::Arg *A : Opts.Args.filtered(options::OPT_I)) {
      const bool IsFrameworkFalse = false;
      const bool IgnoreSysRoot = true;
      if (dxcutil::IsAbsoluteOrCurDirRelative(A->getValue())) {
        HSOpts.AddPath(A->getValue(), frontend::Angled, IsFrameworkFalse, IgnoreSysRoot);
      }
      else {
        std::string s("./");
        s += A->getValue();
        HSOpts.AddPath(s, frontend::Angled, IsFrameworkFalse, IgnoreSysRoot);
      }
    }

    // Apply root signature option.
    unsigned rootSigMinor;
    if (Opts.ForceRootSigVer.empty() || Opts.ForceRootSigVer == "rootsig_1_1") {
      rootSigMinor = 1;
    }
    else {
      DXASSERT(Opts.ForceRootSigVer == "rootsig_1_0",
               "else opts should have been rejected");
      rootSigMinor = 0;
    }
    compiler.getLangOpts().RootSigMajor = 1;
    compiler.getLangOpts().RootSigMinor = rootSigMinor;
    compiler.getLangOpts().HLSLVersion = (unsigned) Opts.HLSLVersion;
    compiler.getLangOpts().EnableDX9CompatMode = Opts.EnableDX9CompatMode;
    compiler.getLangOpts().EnableFXCCompatMode = Opts.EnableFXCCompatMode;

    compiler.getLangOpts().UseMinPrecision = !Opts.Enable16BitTypes;

// SPIRV change starts
#ifdef ENABLE_SPIRV_CODEGEN
    compiler.getLangOpts().SPIRV = Opts.GenSPIRV;
#endif
// SPIRV change ends

    if (Opts.WarningAsError)
      compiler.getDiagnostics().setWarningsAsErrors(true);

    if (Opts.IEEEStrict)
      compiler.getCodeGenOpts().UnsafeFPMath = true;

    if (Opts.FloatDenormalMode.empty()) {
      compiler.getCodeGenOpts().HLSLFloat32DenormMode = DXIL::Float32DenormMode::Reserve7; // undefined
    }
    else if (Opts.FloatDenormalMode.equals_lower(StringRef("any"))) {
      compiler.getCodeGenOpts().HLSLFloat32DenormMode = DXIL::Float32DenormMode::Any;
    }
    else if (Opts.FloatDenormalMode.equals_lower(StringRef("ftz"))) {
      compiler.getCodeGenOpts().HLSLFloat32DenormMode = DXIL::Float32DenormMode::FTZ;
    }
    else {
      DXASSERT(Opts.FloatDenormalMode.equals_lower(StringRef("preserve")), "else opts should have been rejected");
      compiler.getCodeGenOpts().HLSLFloat32DenormMode = DXIL::Float32DenormMode::Preserve;
    }

    if (Opts.DisableOptimizations)
      compiler.getCodeGenOpts().DisableLLVMOpts = true;

    compiler.getCodeGenOpts().OptimizationLevel = Opts.OptLevel;
    if (Opts.OptLevel >= 3)
      compiler.getCodeGenOpts().UnrollLoops = true;

    compiler.getCodeGenOpts().HLSLHighLevel = Opts.CodeGenHighLevel;
    compiler.getCodeGenOpts().HLSLAllowPreserveValues = Opts.AllowPreserveValues;
    compiler.getCodeGenOpts().HLSLResMayAlias = Opts.ResMayAlias;
    compiler.getCodeGenOpts().ScanLimit = Opts.ScanLimit;
    compiler.getCodeGenOpts().HLSLOptimizationToggles = Opts.DxcOptimizationToggles;
    compiler.getCodeGenOpts().HLSLOptimizationSelects = Opts.DxcOptimizationSelects;
    compiler.getCodeGenOpts().HLSLAllResourcesBound = Opts.AllResourcesBound;
    compiler.getCodeGenOpts().HLSLDefaultRowMajor = Opts.DefaultRowMajor;
    compiler.getCodeGenOpts().HLSLPreferControlFlow = Opts.PreferFlowControl;
    compiler.getCodeGenOpts().HLSLAvoidControlFlow = Opts.AvoidFlowControl;
    compiler.getCodeGenOpts().HLSLNotUseLegacyCBufLoad = Opts.NotUseLegacyCBufLoad;
    compiler.getCodeGenOpts().HLSLLegacyResourceReservation = Opts.LegacyResourceReservation;
    compiler.getCodeGenOpts().HLSLDefines = defines;
    compiler.getCodeGenOpts().HLSLPreciseOutputs = Opts.PreciseOutputs;
    compiler.getCodeGenOpts().MainFileName = pMainFile;
    compiler.getCodeGenOpts().HLSLPrintAfterAll = Opts.PrintAfterAll;

    // Translate signature packing options
    if (Opts.PackPrefixStable)
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::PrefixStable;
    else if (Opts.PackOptimized)
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::Optimized;
    else
      compiler.getCodeGenOpts().HLSLSignaturePackingStrategy = (unsigned)DXIL::PackingStrategy::Default;

    // Constructing vector of wide strings to pass in to codegen. Just passing
    // in pArguments will expose ownership of memory to both CodeGenOptions and
    // this caller, which can lead to unexpected behavior.
    for (UINT32 i = 0; i != Opts.Args.getNumInputArgStrings(); ++i) {
      auto arg = Opts.Args.getArgString(i);
      if (Opts.InputFile.compare(arg) != 0) {
        compiler.getCodeGenOpts().HLSLArguments.emplace_back(arg);
      }
    }
    // Overrding default set of loop unroll.
    if (Opts.PreferFlowControl)
      compiler.getCodeGenOpts().UnrollLoops = false;
    if (Opts.AvoidFlowControl)
      compiler.getCodeGenOpts().UnrollLoops = true;

    // always inline for hlsl
    compiler.getCodeGenOpts().setInlining(
        clang::CodeGenOptions::OnlyAlwaysInlining);

    compiler.getCodeGenOpts().HLSLExtensionsCodegen = std::make_shared<HLSLExtensionsCodegenHelperImpl>(compiler, m_langExtensionsHelper, Opts.RootSignatureDefine);

    // AutoBindingSpace also enables automatic binding for libraries if set. UINT_MAX == unset
    compiler.getCodeGenOpts().HLSLDefaultSpace = Opts.AutoBindingSpace;

    // processed export names from -exports option:
    compiler.getCodeGenOpts().HLSLLibraryExports = Opts.Exports;

    // only export shader functions for library
    compiler.getCodeGenOpts().ExportShadersOnly = Opts.ExportShadersOnly;

    if (Opts.DefaultLinkage.empty()) {
      compiler.getCodeGenOpts().DefaultLinkage = DXIL::DefaultLinkage::Default;
    } else if (Opts.DefaultLinkage.equals_lower("internal")) {
      compiler.getCodeGenOpts().DefaultLinkage = DXIL::DefaultLinkage::Internal;
    } else if (Opts.DefaultLinkage.equals_lower("external")) {
      compiler.getCodeGenOpts().DefaultLinkage = DXIL::DefaultLinkage::External;
    }
  }

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) override {
    if (pMajor == nullptr || pMinor == nullptr)
      return E_INVALIDARG;
    *pMajor = DXIL::kDxilMajor;
    *pMinor = DXIL::kDxilMinor;
    return S_OK;
  }
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
  HRESULT STDMETHODCALLTYPE GetCommitInfo(_Out_ UINT32 *pCommitCount,
                                          _Out_ char **pCommitHash) override {
    if (pCommitCount == nullptr || pCommitHash == nullptr)
      return E_INVALIDARG;

    char *const hash = (char *)CoTaskMemAlloc(8 + 1); // 8 is guaranteed by utils/GetCommitInfo.py
    if (hash == nullptr)
      return E_OUTOFMEMORY;
    std::strcpy(hash, getGitCommitHash());

    *pCommitHash = hash;
    *pCommitCount = getGitCommitCount();

    return S_OK;
  }
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
  HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) override {
    if (pFlags == nullptr)
      return E_INVALIDARG;
    *pFlags = DxcVersionInfoFlags_None;
#ifdef _DEBUG
    *pFlags |= DxcVersionInfoFlags_Debug;
#endif
    return S_OK;
  }
};

//////////////////////////////////////////////////////////////
// legacy IDxcCompiler2 implementation that maps to DxcCompiler
ULONG STDMETHODCALLTYPE DxcCompilerAdapter::AddRef() {
  return m_pCompilerImpl->AddRef();
}
ULONG STDMETHODCALLTYPE DxcCompilerAdapter::Release() {
  return m_pCompilerImpl->Release();
}
HRESULT STDMETHODCALLTYPE DxcCompilerAdapter::QueryInterface(REFIID iid, void **ppvObject) {
   return m_pCompilerImpl->QueryInterface(iid, ppvObject);
}

// Preprocess source text
HRESULT STDMETHODCALLTYPE DxcCompilerAdapter::Preprocess(
  _In_ IDxcBlob *pSource,                       // Source text to preprocess
  _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
  _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
  _In_ UINT32 argCount,                         // Number of arguments
  _In_count_(defineCount)
    const DxcDefine *pDefines,                  // Array of defines
  _In_ UINT32 defineCount,                      // Number of defines
  _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
  _COM_Outptr_ IDxcOperationResult **ppResult   // Preprocessor output status, buffer, and errors
) {
  if (pSource == nullptr || ppResult == nullptr ||
      (defineCount > 0 && pDefines == nullptr) ||
      (argCount > 0 && pArguments == nullptr))
    return E_INVALIDARG;
  *ppResult = nullptr;

  return WrapCompile(
    TRUE,
    pSource, pSourceName,
    nullptr, nullptr,
    pArguments, argCount,
    pDefines, defineCount,
    pIncludeHandler,
    ppResult, nullptr, nullptr);
}

// Disassemble a program.
HRESULT STDMETHODCALLTYPE DxcCompilerAdapter::Disassemble(
  _In_ IDxcBlob *pProgram,                        // Program to disassemble.
  _COM_Outptr_ IDxcBlobEncoding **ppDisassembly   // Disassembly text.
) {
  if (pProgram == nullptr || ppDisassembly == nullptr)
    return E_INVALIDARG;

  *ppDisassembly = nullptr;

  HRESULT hr = S_OK;
  DxcThreadMalloc TM(m_pMalloc);

  DxcBuffer buffer = { pProgram->GetBufferPointer(), pProgram->GetBufferSize(), 0 };
  CComPtr<IDxcResult> pResult;
  IFR(m_pCompilerImpl->Disassemble(&buffer, IID_PPV_ARGS(&pResult)));
  IFRBOOL(pResult, E_OUTOFMEMORY);

  IFR(pResult->GetStatus(&hr));
  if (SUCCEEDED(hr)) {
    // Extract disassembly
    IFR(pResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(ppDisassembly), nullptr));
  }
  return hr;
}

HRESULT CreateDxcUtils(_In_ REFIID riid, _Out_ LPVOID *ppv);

HRESULT STDMETHODCALLTYPE DxcCompilerAdapter::CompileWithDebug(
  _In_ IDxcBlob *pSource,                       // Source text to compile
  _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
  _In_ LPCWSTR pEntryPoint,                     // Entry point name
  _In_ LPCWSTR pTargetProfile,                  // Shader profile to compile
  _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
  _In_ UINT32 argCount,                         // Number of arguments
  _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
  _In_ UINT32 defineCount,                      // Number of defines
  _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
  _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
  _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob.
  _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
) {
  if (pSource == nullptr || ppResult == nullptr ||
      (defineCount > 0 && pDefines == nullptr) ||
      (argCount > 0 && pArguments == nullptr) ||
      pTargetProfile == nullptr)
    return E_INVALIDARG;

  *ppResult = nullptr;
  AssignToOutOpt(nullptr, ppDebugBlobName);
  AssignToOutOpt(nullptr, ppDebugBlob);

  return WrapCompile(
    FALSE,
    pSource, pSourceName,
    pEntryPoint, pTargetProfile,
    pArguments, argCount,
    pDefines, defineCount,
    pIncludeHandler,
    ppResult, ppDebugBlobName, ppDebugBlob);
}

HRESULT DxcCompilerAdapter::WrapCompile(
  _In_ BOOL bPreprocess,                        // Preprocess mode
  _In_ IDxcBlob *pSource,                       // Source text to compile
  _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
  _In_ LPCWSTR pEntryPoint,                     // Entry point name
  _In_ LPCWSTR pTargetProfile,                  // Shader profile to compile
  _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
  _In_ UINT32 argCount,                         // Number of arguments
  _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
  _In_ UINT32 defineCount,                      // Number of defines
  _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
  _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
  _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob.
  _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
) {
  HRESULT hr = S_OK;
  DxcThreadMalloc TM(m_pMalloc);

  try {
    CComPtr<IDxcUtils> pUtils;
    IFT(CreateDxcUtils(IID_PPV_ARGS(&pUtils)));
    CComPtr<IDxcCompilerArgs> pArgs;
    IFR(pUtils->BuildArguments(
      pSourceName, pEntryPoint, pTargetProfile,
      pArguments, argCount, pDefines, defineCount, &pArgs));

    LPCWSTR PreprocessArgs[] = { L"-P", L"preprocessed.hlsl" };
    if (bPreprocess) {
      IFT(pArgs->AddArguments(PreprocessArgs, _countof(PreprocessArgs)));
    }

    DxcBuffer buffer = { pSource->GetBufferPointer(), pSource->GetBufferSize(), CP_ACP };
    CComPtr<IDxcBlobEncoding> pSourceEncoding;
    if (SUCCEEDED(pSource->QueryInterface(&pSourceEncoding))) {
      BOOL sourceEncodingKnown = false;
      IFT(pSourceEncoding->GetEncoding(&sourceEncodingKnown, &buffer.Encoding));
    }

    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(m_pMalloc, &pOutputStream));

    // Parse command-line options into DxcOpts
    int argCountInt;
    IFT(UIntToInt(pArgs->GetCount(), &argCountInt));
    hlsl::options::MainArgs mainArgs(argCountInt, pArgs->GetArguments(), 0);
    hlsl::options::DxcOpts opts;
    bool finished = false;
    CComPtr<IDxcOperationResult> pOperationResult;
    dxcutil::ReadOptsAndValidate(mainArgs, opts, pOutputStream, &pOperationResult, finished);
    if (finished) {
      IFT(pOperationResult->QueryInterface(ppResult));
      return S_OK;
    }

    if (pOutputStream->GetPosition() > 0) {
      // Clear existing stream in case it has option spew
      pOutputStream.Release();
      IFT(CreateMemoryStream(m_pMalloc, &pOutputStream));
    }

    // To concat out output with compiler errors
    raw_stream_ostream outStream(pOutputStream);

    LPCWSTR EmbedDebugOpt[] = { L"-Qembed_debug" };
    if (opts.DebugInfo && !ppDebugBlob && !opts.EmbedDebug && !opts.StripDebug) {
// SPIRV change starts
#if defined(ENABLE_SPIRV_CODEGEN)
      if (!opts.GenSPIRV)
        outStream << "warning: no output provided for debug - embedding PDB in "
                     "shader container.  Use -Qembed_debug to silence this "
                     "warning.\n";
#else
      outStream << "warning: no output provided for debug - embedding PDB in "
                   "shader container.  Use -Qembed_debug to silence this "
                   "warning.\n";
#endif
// SPIRV change ends
      IFT(pArgs->AddArguments(EmbedDebugOpt, _countof(EmbedDebugOpt)));
    }

    CComPtr<DxcResult> pResult = DxcResult::Alloc(m_pMalloc);
    pResult->SetEncoding(opts.DefaultTextCodePage);

    CComPtr<IDxcResult> pImplResult;
    IFR(m_pCompilerImpl->Compile(&buffer, pArgs->GetArguments(), pArgs->GetCount(), pIncludeHandler, IID_PPV_ARGS(&pImplResult)));
    IFRBOOL(pImplResult, E_OUTOFMEMORY);
    IFR(pImplResult->GetStatus(&hr));

    pResult->CopyOutputsFromResult(pImplResult);
    pResult->SetStatusAndPrimaryResult(hr, pImplResult->PrimaryOutput());

    outStream.flush();

    // Insert any warnings generated here
    if (pOutputStream->GetPosition() > 0) {
      CComPtr<IDxcBlobEncoding> pErrorsEncoding;
      if (SUCCEEDED(pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrorsEncoding), nullptr)) &&
          pErrorsEncoding && pErrorsEncoding->GetBufferSize()) {
        CComPtr<IDxcBlobUtf8> pErrorsUtf8;
        IFT(pUtils->GetBlobAsUtf8(pErrorsEncoding, &pErrorsUtf8));
        outStream << pErrorsUtf8->GetStringPointer();
        outStream.flush();
      }
      // Reconstruct result with new error buffer
      CComPtr<IDxcBlobEncoding> pErrorBlob;
      IFT(hlsl::DxcCreateBlob(
        pOutputStream->GetPtr(), pOutputStream->GetPtrSize(),
        false, true, true, DXC_CP_UTF8, nullptr, &pErrorBlob));
      if (pErrorBlob && pErrorBlob->GetBufferSize()) {
        pResult->Output(DXC_OUT_ERRORS)->object.Release();
        pResult->SetOutputObject(DXC_OUT_ERRORS, pErrorBlob);
      }
    }

    // Extract debug blob if present
    CComHeapPtr<wchar_t> pDebugNameOnComHeap;
    CComPtr<IDxcBlob> pDebugBlob;
    if (SUCCEEDED(hr)) {
      CComPtr<IDxcBlobUtf16> pDebugName;
      hr = pResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pDebugBlob), &pDebugName);
      if (SUCCEEDED(hr) && ppDebugBlobName && pDebugName) {
        if (!pDebugNameOnComHeap.AllocateBytes(pDebugName->GetBufferSize()))
          return E_OUTOFMEMORY;
        memcpy(pDebugNameOnComHeap.m_pData, pDebugName->GetBufferPointer(), pDebugName->GetBufferSize());
      }
    }

    if (ppDebugBlob && pDebugBlob)
      *ppDebugBlob = pDebugBlob.Detach();
    if (ppDebugBlobName && pDebugNameOnComHeap)
      *ppDebugBlobName = pDebugNameOnComHeap.Detach();

    IFR(pResult.QueryInterface(ppResult));
    hr = S_OK;
  } catch (std::bad_alloc &) {
    hr = E_OUTOFMEMORY;
  } catch (hlsl::Exception &e) {
    _Analysis_assume_(DXC_FAILED(e.hr));
    hr = DxcResult::Create(e.hr, DXC_OUT_NONE, {
        DxcOutputObject::ErrorOutput(CP_UTF8, e.msg.c_str(), e.msg.size())
      }, ppResult);
  } catch (...) {
    hr = E_FAIL;
  }
  return hr;
}
//////////////////////////////////////////////////////////////


HRESULT CreateDxcCompiler(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  *ppv = nullptr;
  try {
    CComPtr<DxcCompiler> result(DxcCompiler::Alloc(DxcGetThreadMallocNoRef()));
    IFROOM(result.p);
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
