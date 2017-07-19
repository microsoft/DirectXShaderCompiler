///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// lib_share_preprocessor.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements preprocessor to split shader based on include.                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "dxc/Support/FileIOHelper.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "clang/Frontend/ASTUnit.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/SemaConsumer.h"
#include "dxc/Support/dxcfilesystem.h"
#include "lib_share_helper.h"

using namespace libshare;

using namespace hlsl;
using namespace llvm;

namespace dxcutil {
bool IsAbsoluteOrCurDirRelative(const Twine &T) {
  if (llvm::sys::path::is_absolute(T)) {
    return true;
  }
  if (T.isSingleStringRef()) {
    StringRef r = T.getSingleStringRef();
    if (r.size() < 2)
      return false;
    const char *pData = r.data();
    return pData[0] == '.' && (pData[1] == '\\' || pData[1] == '/');
  }
  DXASSERT(false, "twine kind not supported");
  return false;
}
}

namespace {
class IncPathIncludeHandler : public IDxcIncludeHandler {
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  virtual ~IncPathIncludeHandler() {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    return DoBasicQueryInterface<::IDxcIncludeHandler>(this, riid, ppvObject);
  }
  IncPathIncludeHandler(IDxcIncludeHandler *handler, std::vector<std::string> &includePathList)
      : m_dwRef(0), m_pIncludeHandler(handler), m_includePathList(includePathList) {}
  __override HRESULT STDMETHODCALLTYPE LoadSource(
      _In_ LPCWSTR pFilename, // Candidate filename.
      _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource // Resultant
                                                               // source object
                                                               // for included
                                                               // file, nullptr
                                                               // if not found.
  ) {
    CW2A pUtf8Filename(pFilename);
    if (m_loadedFileNames.count(pUtf8Filename.m_psz)) {
      // Already include this file.
      // Just return empty content.
      static const char kEmptyStr[] = " ";
      CComPtr<IDxcBlobEncoding> pEncodingIncludeSource;
      IFT(DxcCreateBlobWithEncodingOnMalloc(kEmptyStr, GetGlobalHeapMalloc(), 1,
                                            CP_UTF8, &pEncodingIncludeSource));
      *ppIncludeSource = pEncodingIncludeSource.Detach();
      return S_OK;
    }
    // Read file.
    CComPtr<IDxcBlob> pIncludeSource;
    if (m_includePathList.empty()) {
      IFT(m_pIncludeHandler->LoadSource(pFilename, ppIncludeSource));
      return S_OK;
    } else {
      bool bLoaded = false;
      // Not support same filename in different directory.
      for (std::string &path : m_includePathList) {
        llvm::Twine tmpFilename = path + pUtf8Filename;
        std::string tmpFilenameStr = tmpFilename.str();
        CA2W pWTmpFilename(tmpFilenameStr.c_str());
        if (S_OK == m_pIncludeHandler->LoadSource(pWTmpFilename.m_psz,
                                                  ppIncludeSource)) {
          bLoaded = true;
          break;
        }
      }
      if (!bLoaded) {
        IFT(m_pIncludeHandler->LoadSource(pFilename, ppIncludeSource));
        return S_OK;
      } else {
        return S_OK;
      }
    }
  }

private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  IDxcIncludeHandler *m_pIncludeHandler;
  StringSet<> m_loadedFileNames;
  std::vector<std::string> &m_includePathList;
};

} // namespace

namespace {

class IncludeToLibPreprocessorImpl : public IncludeToLibPreprocessor {
public:
  virtual ~IncludeToLibPreprocessorImpl() {}

  IncludeToLibPreprocessorImpl(IDxcIncludeHandler *handler)
      : m_pIncludeHandler(handler) {
    if (!m_dllSupport.IsEnabled())
      m_dllSupport.Initialize();
  }

  void SetupDefines(const DxcDefine *pDefines, unsigned defineCount) override;
  void AddIncPath(StringRef path) override;
  HRESULT Preprocess(IDxcBlob *pSource, LPCWSTR pFilename) override;

  const std::vector<std::string> &GetHeaders() const override { return m_headers; }

private:
  HRESULT RewriteToNoFuncBody(IDxcRewriter *pRewriter, LPCWSTR pFilename,
    std::string &Source, IDxcBlob **ppNoFuncBodySource);
  IDxcIncludeHandler *m_pIncludeHandler;
  // Processed header content.
  std::vector<std::string> m_headers;
  // Defines.
  std::vector<std::wstring> m_defineStrs;
  std::vector<DxcDefine> m_defines;
  std::vector<std::string> m_includePathList;
  dxc::DxcDllSupport m_dllSupport;
};

void IncludeToLibPreprocessorImpl::SetupDefines(const DxcDefine *pDefines,
                                            unsigned defineCount) {
  // Make sure no resize.
  m_defineStrs.reserve(defineCount * 2);
  m_defines.reserve(defineCount);
  for (unsigned i = 0; i < defineCount; i++) {
    const DxcDefine &define = pDefines[i];

    m_defineStrs.emplace_back(define.Name);
    DxcDefine tmpDefine;
    tmpDefine.Name = m_defineStrs.back().c_str();
    tmpDefine.Value = nullptr;
    if (define.Value) {
      m_defineStrs.emplace_back(define.Value);
      tmpDefine.Value = m_defineStrs.back().c_str();
    }
    m_defines.emplace_back(tmpDefine);
  }
}

void IncludeToLibPreprocessorImpl::AddIncPath(StringRef path) {
  m_includePathList.emplace_back(path);
}

HRESULT IncludeToLibPreprocessorImpl::RewriteToNoFuncBody(IDxcRewriter *pRewriter, LPCWSTR pFilename,
    std::string &Source, IDxcBlob **ppNoFuncBodySource) {
  // Create header with no function body.
  CComPtr<IDxcBlobEncoding> pEncodingIncludeSource;
  IFR(DxcCreateBlobWithEncodingOnMalloc(
      Source.data(), GetGlobalHeapMalloc(), Source.size(),
      CP_UTF8, &pEncodingIncludeSource));

  CComPtr<IDxcOperationResult> pRewriteResult;
  IFT(pRewriter->RewriteUnchangedWithInclude(
      pEncodingIncludeSource, pFilename, m_defines.data(), m_defines.size(),
      // Don't need include handler here, include already read in
      // RewriteIncludesToSnippet
      nullptr,
      RewriterOptionMask::SkipFunctionBody | RewriterOptionMask::KeepUserMacro,
      &pRewriteResult));
  // includeSource ownes the memory.
  pEncodingIncludeSource.Detach();
  HRESULT status;
  if (!SUCCEEDED(pRewriteResult->GetStatus(&status)) || !SUCCEEDED(status)) {
    CComPtr<IDxcBlobEncoding> pErr;
    IFT(pRewriteResult->GetErrorBuffer(&pErr));
    std::string errString =
        std::string((char *)pErr->GetBufferPointer(), pErr->GetBufferSize());
    IFTMSG(E_FAIL, errString);
    return E_FAIL;
  };

  // Get result.
  IFT(pRewriteResult->GetResult(ppNoFuncBodySource));
  return S_OK;
}

using namespace clang;
static
void SetupCompilerForRewrite(CompilerInstance &compiler,
                             _In_ DxcLangExtensionsHelperApply *helper,
                             _In_ LPCSTR pMainFile,
                             _In_ TextDiagnosticPrinter *diagPrinter,
                             _In_opt_ clang::ASTUnit::RemappedFile *rewrite,
                             _In_opt_ LPCSTR pDefines) {
  // Setup a compiler instance.
  std::shared_ptr<TargetOptions> targetOptions(new TargetOptions);
  targetOptions->Triple = llvm::sys::getDefaultTargetTriple();
  compiler.HlslLangExtensions = helper;
  compiler.createDiagnostics(diagPrinter, false);
  compiler.createFileManager();
  compiler.createSourceManager(compiler.getFileManager());
  compiler.setTarget(TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), targetOptions));
  // Not use builtin includes.
  compiler.getHeaderSearchOpts().UseBuiltinIncludes = false;

  PreprocessorOptions &PPOpts = compiler.getPreprocessorOpts();
  if (rewrite != nullptr) {
    if (llvm::MemoryBuffer *pMemBuf = rewrite->second) {
      compiler.getPreprocessorOpts().addRemappedFile(StringRef(pMainFile), pMemBuf);
    }

    PPOpts.RemappedFilesKeepOriginalName = true;
  }

  PreprocessorOutputOptions &PPOutOpts = compiler.getPreprocessorOutputOpts();
  PPOutOpts.ShowLineMarkers = false;

  compiler.createPreprocessor(TU_Complete);

  if (pDefines) {
    std::string newDefines = compiler.getPreprocessor().getPredefines();
    newDefines += pDefines;
    compiler.getPreprocessor().setPredefines(newDefines);
  }

  compiler.createASTContext();
  compiler.setASTConsumer(std::unique_ptr<ASTConsumer>(new SemaConsumer()));
  compiler.createSema(TU_Complete, nullptr);

  const FileEntry *mainFileEntry = compiler.getFileManager().getFile(StringRef(pMainFile));
  if (mainFileEntry == nullptr) {
    throw ::hlsl::Exception(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
  }
  compiler.getSourceManager().setMainFileID(
    compiler.getSourceManager().createFileID(mainFileEntry, SourceLocation(), SrcMgr::C_User));
}

static std::string DefinesToString(_In_count_(defineCount) DxcDefine *pDefines,
                                   _In_ UINT32 defineCount) {
  std::string defineStr;
  for (UINT32 i = 0; i < defineCount; i++) {
    CW2A utf8Name(pDefines[i].Name, CP_UTF8);
    CW2A utf8Value(pDefines[i].Value, CP_UTF8);
    defineStr += "#define ";
    defineStr += utf8Name;
    defineStr += " ";
    defineStr += utf8Value ? utf8Value : "1";
    defineStr += "\n";
  }

  return defineStr;
}

static void RewriteToSnippets(IDxcBlob *pSource, LPCWSTR pFilename,
    std::vector<DxcDefine> &defines, IDxcIncludeHandler *pIncHandler,
    std::vector<std::string> &Snippets) {
  std::string warnings;
  raw_string_ostream w(warnings);

  CW2A pUtf8Name(pFilename);
  dxcutil::DxcArgsFileSystem *msfPtr =
      dxcutil::CreateDxcArgsFileSystem(pSource, pFilename, pIncHandler);
  std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);

  ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
  IFTLLVM(pts.error_code());

  StringRef Data((LPSTR)pSource->GetBufferPointer(), pSource->GetBufferSize());
  std::unique_ptr<llvm::MemoryBuffer> pBuffer(
      llvm::MemoryBuffer::getMemBufferCopy(Data, pUtf8Name.m_psz));
  std::unique_ptr<clang::ASTUnit::RemappedFile> pRemap(
      new clang::ASTUnit::RemappedFile(pUtf8Name.m_psz, pBuffer.release()));

  std::string definesStr = DefinesToString(defines.data(), defines.size());
  // Not support langExt yet.
  DxcLangExtensionsHelperApply *pLangExtensionsHelper = nullptr;
  // Setup a compiler instance.
  clang::CompilerInstance compiler;
  std::unique_ptr<clang::TextDiagnosticPrinter> diagPrinter =
      std::make_unique<clang::TextDiagnosticPrinter>(
          w, &compiler.getDiagnosticOpts());
  SetupCompilerForRewrite(compiler, pLangExtensionsHelper, pUtf8Name.m_psz,
                          diagPrinter.get(), pRemap.get(), definesStr.c_str());

  // Parse the source file.
  compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),
                                                 &compiler.getPreprocessor());

  clang::RewriteIncludesToSnippet(compiler.getPreprocessor(),
                                  compiler.getPreprocessorOutputOpts(),
                                  Snippets);
}

HRESULT IncludeToLibPreprocessorImpl::Preprocess(IDxcBlob *pSource,
                                             LPCWSTR pFilename) {
  std::string s, warnings;
  raw_string_ostream o(s);
  raw_string_ostream w(warnings);

  CW2A pUtf8Name(pFilename);

  IncPathIncludeHandler incPathIncludeHandler(m_pIncludeHandler, m_includePathList);
  // AddRef to hold incPathIncludeHandler.
  // If not, DxcArgsFileSystem will kill it.
  incPathIncludeHandler.AddRef();
  std::vector<std::string> Snippets;
  RewriteToSnippets(pSource, pFilename, m_defines, &incPathIncludeHandler, Snippets);

  // Combine Snippets.
  CComPtr<IDxcRewriter> pRewriter;
  m_dllSupport.CreateInstance(CLSID_DxcRewriter, &pRewriter);
  std::string curHeader = "";
  for (std::string &Snippet : Snippets) {
    curHeader = curHeader + Snippet;
    m_headers.emplace_back(curHeader);
    // Rewrite curHeader to remove function body.
    CComPtr<IDxcBlob> result;
    IFT(RewriteToNoFuncBody(pRewriter, L"input.hlsl", curHeader, &result));
    curHeader = std::string((char *)(result)->GetBufferPointer(),
                                   (result)->GetBufferSize());
  }

  return S_OK;
}
} // namespace

std::unique_ptr<IncludeToLibPreprocessor>
  IncludeToLibPreprocessor::CreateIncludeToLibPreprocessor(IDxcIncludeHandler *handler) {
  return std::make_unique<IncludeToLibPreprocessorImpl>(handler);
}