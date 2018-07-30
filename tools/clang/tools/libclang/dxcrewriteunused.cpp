///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcrewriteunused.cpp                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Compiler rewriter for unused data and functions.   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/Host.h"
#include "clang/Sema/SemaHLSL.h"

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"

#include "dxc/dxcapi.internal.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/DxcLangExtensionsHelper.h"
#include "dxc/Support/dxcfilesystem.h"

#define CP_UTF16 1200

using namespace llvm;
using namespace clang;
using namespace hlsl;

class RewriteUnusedASTConsumer : public SemaConsumer {
private:
  Sema* m_sema = nullptr;
public:
  RewriteUnusedASTConsumer() {
  }
  void InitializeSema(Sema& S) override {
    m_sema = &S;
  }
  void ForgetSema() override {
    m_sema = nullptr;
  }
};

class VarReferenceVisitor : public RecursiveASTVisitor<VarReferenceVisitor> {
private:
  SmallPtrSet<VarDecl*, 128>& m_unusedGlobals;
  SmallPtrSet<FunctionDecl*, 128>& m_visitedFunctions;
  SmallVector<FunctionDecl*, 32>& m_pendingFunctions;
public:
  VarReferenceVisitor(
    SmallPtrSet<VarDecl*, 128>& unusedGlobals,
    SmallPtrSet<FunctionDecl*, 128>& visitedFunctions,
    SmallVector<FunctionDecl*, 32>& pendingFunctions) :
    m_unusedGlobals(unusedGlobals),
    m_visitedFunctions(visitedFunctions),
    m_pendingFunctions(pendingFunctions) {
  }

  bool VisitDeclRefExpr(DeclRefExpr* ref) {
    ValueDecl* valueDecl = ref->getDecl();
    FunctionDecl* fnDecl = dyn_cast_or_null<FunctionDecl>(valueDecl);
    if (fnDecl != nullptr) {
      if (!m_visitedFunctions.count(fnDecl)) {
        m_pendingFunctions.push_back(fnDecl);
      }
    }
    else {
      VarDecl* varDecl = dyn_cast_or_null<VarDecl>(valueDecl);
      if (varDecl != nullptr) {
        m_unusedGlobals.erase(varDecl);
      }
    }
    return true;
  }
};

static void raw_string_ostream_to_CoString(raw_string_ostream &o, _Outptr_result_z_ LPSTR *pResult) {
  std::string& s = o.str(); // .str() will flush automatically
  *pResult = (LPSTR)CoTaskMemAlloc(s.size() + 1);
  if (*pResult == nullptr) 
    throw std::bad_alloc();
  strncpy(*pResult, s.c_str(), s.size() + 1);
}

static
void SetupCompilerForRewrite(CompilerInstance &compiler,
                             _In_ DxcLangExtensionsHelper *helper,
                             _In_ LPCSTR pMainFile,
                             _In_ TextDiagnosticPrinter *diagPrinter,
                             _In_opt_ ASTUnit::RemappedFile *rewrite,
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

static bool IsMacroMatch(StringRef name, const std::string &mask) {
  return Unicode::IsStarMatchUTF8(mask.c_str(), mask.size(), name.data(),
                                  name.size());
}

static
bool MacroPairCompareIsLessThan(const std::pair<const IdentifierInfo*, const MacroInfo*> &left,
                                const std::pair<const IdentifierInfo*, const MacroInfo*> &right) {
  return left.first->getName().compare(right.first->getName()) < 0;
}


static
void WriteMacroDefines(ParsedSemanticDefineList &macros, raw_string_ostream &o) {
  if (!macros.empty()) {
    o << "\n// Macros:\n";
    for (auto&& m : macros) {
      o << "#define " << m.Name << " " << m.Value << "\n";
    }
  }
}

static
void WriteSemanticDefines(CompilerInstance &compiler, _In_ DxcLangExtensionsHelper *helper, raw_string_ostream &o) {
  ParsedSemanticDefineList macros = CollectSemanticDefinesParsedByCompiler(compiler, helper);
  WriteMacroDefines(macros, o);
}

ParsedSemanticDefineList hlsl::CollectSemanticDefinesParsedByCompiler(CompilerInstance &compiler, _In_ DxcLangExtensionsHelper *helper) {
  ParsedSemanticDefineList parsedDefines;
  const llvm::SmallVector<std::string, 2>& defines = helper->GetSemanticDefines();
  if (defines.size() == 0) {
    return parsedDefines;
  }

  const llvm::SmallVector<std::string, 2>& defineExclusions = helper->GetSemanticDefineExclusions();

  // This is very inefficient in general, but in practice we either have
  // no semantic defines, or we have a star define for a some reserved prefix. These will be
  // sorted so rewrites are stable.
  std::vector<std::pair<const IdentifierInfo*, MacroInfo*> > macros;
  Preprocessor& pp = compiler.getPreprocessor();
  Preprocessor::macro_iterator end = pp.macro_end();
  for (Preprocessor::macro_iterator i = pp.macro_begin(); i != end; ++i) {
    if (!i->second.getLatest()->isDefined()) {
      continue;
    }
    MacroInfo* mi = i->second.getLatest()->getMacroInfo();
    if (mi->isFunctionLike()) {
      continue;
    }

    const IdentifierInfo* ii = i->first;

    // Exclusions take precedence over inclusions.
    bool excluded = false;
    for (const auto &exclusion : defineExclusions) {
      if (IsMacroMatch(ii->getName(), exclusion)) {
        excluded = true;
        break;
      }
    }
    if (excluded) {
      continue;
    }

    for (const auto &define : defines) {
      if (!IsMacroMatch(ii->getName(), define)) {
        continue;
      }

      macros.push_back(std::pair<const IdentifierInfo*, MacroInfo*>(ii, mi));
    }
  }

  if (!macros.empty()) {
    std::sort(macros.begin(), macros.end(), MacroPairCompareIsLessThan);
    MacroExpander expander(pp);
    for (std::pair<const IdentifierInfo *, MacroInfo *> m : macros) {
      std::string expandedValue;
      expander.ExpandMacro(m.second, &expandedValue);
      parsedDefines.emplace_back(ParsedSemanticDefine{ m.first->getName(), expandedValue, m.second->getDefinitionLoc().getRawEncoding() });
    }
  }

  return parsedDefines;
}

static ParsedSemanticDefineList CollectUserMacrosParsedByCompiler(CompilerInstance &compiler) {
  ParsedSemanticDefineList parsedDefines;
  // This is very inefficient in general, but in practice we either have
  // no semantic defines, or we have a star define for a some reserved prefix. These will be
  // sorted so rewrites are stable.
  std::vector<std::pair<const IdentifierInfo*, MacroInfo*> > macros;
  Preprocessor& pp = compiler.getPreprocessor();
  Preprocessor::macro_iterator end = pp.macro_end();
  SourceManager &SM = compiler.getSourceManager();
  FileID PredefineFileID = pp.getPredefinesFileID();

  for (Preprocessor::macro_iterator i = pp.macro_begin(); i != end; ++i) {
    if (!i->second.getLatest()->isDefined()) {
      continue;
    }
    MacroInfo* mi = i->second.getLatest()->getMacroInfo();
    if (mi->getDefinitionLoc().isInvalid()) {
      continue;
    }
    FileID FID = SM.getFileID(mi->getDefinitionEndLoc());
    if (FID == PredefineFileID)
      continue;

    const IdentifierInfo* ii = i->first;

    macros.push_back(std::pair<const IdentifierInfo*, MacroInfo*>(ii, mi));
  }

  if (!macros.empty()) {
    std::sort(macros.begin(), macros.end(), MacroPairCompareIsLessThan);
    MacroExpander expander(pp);
    for (std::pair<const IdentifierInfo *, MacroInfo *> m : macros) {
      std::string expandedValue;
      MacroInfo* mi = m.second;
      if (!mi->isFunctionLike()) {
        expander.ExpandMacro(m.second, &expandedValue);
        parsedDefines.emplace_back(ParsedSemanticDefine{ m.first->getName(), expandedValue, m.second->getDefinitionLoc().getRawEncoding() });
      } else {
        std::string macroStr;
        raw_string_ostream macro(macroStr);
        macro << m.first->getName();
        auto args = mi->args();

        macro << "(";
        for (unsigned I = 0; I != mi->getNumArgs(); ++I) {
          if (I)
            macro << ", ";
          macro << args[I]->getName();
        }
        macro << ")";
        macro.flush();

        std::string macroValStr;
        raw_string_ostream macroVal(macroValStr);
        for (const Token &Tok : mi->tokens()) {
          macroVal << " ";
          if (const char *Punc = tok::getPunctuatorSpelling(Tok.getKind()))
            macroVal << Punc;
          else if (const char *Kwd = tok::getKeywordSpelling(Tok.getKind()))
            macroVal << Kwd;
          else if (Tok.is(tok::identifier))
            macroVal << Tok.getIdentifierInfo()->getName();
          else if (Tok.isLiteral() && Tok.getLiteralData())
            macroVal << StringRef(Tok.getLiteralData(), Tok.getLength());
          else
            macroVal << Tok.getName();
        }
        macroVal.flush();
        parsedDefines.emplace_back(ParsedSemanticDefine{ macroStr, macroValStr, m.second->getDefinitionLoc().getRawEncoding() });
      }
    }
  }

  return parsedDefines;
}


static
void WriteUserMacroDefines(CompilerInstance &compiler, raw_string_ostream &o) {
  ParsedSemanticDefineList macros = CollectUserMacrosParsedByCompiler(compiler);
  WriteMacroDefines(macros, o);
}

static
HRESULT DoRewriteUnused(_In_ DxcLangExtensionsHelper *pHelper,
                     _In_ LPCSTR pFileName,
                     _In_ ASTUnit::RemappedFile *pRemap,
                     _In_ LPCSTR pEntryPoint,
                     _In_ LPCSTR pDefines,
                     _Outptr_result_z_ LPSTR *pWarnings,
                     _Outptr_result_z_ LPSTR *pResult) {
  if (pWarnings != nullptr) *pWarnings = nullptr;
  if (pResult != nullptr) *pResult = nullptr;

  std::string s, warnings;
  raw_string_ostream o(s);
  raw_string_ostream w(warnings);

  // Setup a compiler instance.
  CompilerInstance compiler;
  std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
      llvm::make_unique<TextDiagnosticPrinter>(w, &compiler.getDiagnosticOpts());  
  SetupCompilerForRewrite(compiler, pHelper, pFileName, diagPrinter.get(), pRemap, pDefines);

  // Parse the source file.
  compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(), &compiler.getPreprocessor());
  ParseAST(compiler.getSema(), false, false);

  ASTContext& C = compiler.getASTContext();
  TranslationUnitDecl *tu = C.getTranslationUnitDecl();

  // Gather all global variables that are not in cbuffers and all functions.
  SmallPtrSet<VarDecl*, 128> unusedGlobals;
  SmallPtrSet<FunctionDecl*, 128> unusedFunctions;
  auto tuDeclsEnd = tu->decls_end();
  for (auto && tuDecl = tu->decls_begin(); tuDecl != tuDeclsEnd; ++tuDecl) {
    VarDecl* varDecl = dyn_cast_or_null<VarDecl>(*tuDecl);
    if (varDecl != nullptr) {
      unusedGlobals.insert(varDecl);
      continue;
    }

    FunctionDecl* fnDecl = dyn_cast_or_null<FunctionDecl>(*tuDecl);
    if (fnDecl != nullptr) {
      if (fnDecl->doesThisDeclarationHaveABody()) {
        unusedFunctions.insert(fnDecl);
      }
    }
  }

  w << "//found " << unusedGlobals.size() << " globals as candidates for removal\n";
  w << "//found " << unusedFunctions.size() << " functions as candidates for removal\n";

  DeclContext::lookup_result l = tu->lookup(DeclarationName(&C.Idents.get(StringRef(pEntryPoint))));
  if (l.empty()) {
    w << "//entry point not found\n";
  }
  else {
    w << "//entry point found\n";
    NamedDecl *entryDecl = l.front();
    FunctionDecl *entryFnDecl = dyn_cast_or_null<FunctionDecl>(entryDecl);
    if (entryFnDecl == nullptr) {
      o << "//entry point found but is not a function declaration\n";
    }
    else {
      // Traverse reachable functions and variables.
      SmallPtrSet<FunctionDecl*, 128> visitedFunctions;
      SmallVector<FunctionDecl*, 32> pendingFunctions;
      VarReferenceVisitor visitor(unusedGlobals, visitedFunctions, pendingFunctions);
      pendingFunctions.push_back(entryFnDecl);
      while (!pendingFunctions.empty() && !unusedGlobals.empty()) {
        FunctionDecl* pendingDecl = pendingFunctions.pop_back_val();
        visitedFunctions.insert(pendingDecl);
        visitor.TraverseDecl(pendingDecl);
      }

      // Don't bother doing work if there are no globals to remove.
      if (unusedGlobals.empty()) {
        w << "//no unused globals found - no work to be done\n";
        StringRef contents = C.getSourceManager().getBufferData(C.getSourceManager().getMainFileID());
        o << contents;
      }
      else {
        w << "//found " << unusedGlobals.size() << " globals to remove\n";

        // Don't remove visited functions.
        for (FunctionDecl *visitedFn : visitedFunctions) {
          unusedFunctions.erase(visitedFn);
        }
        w << "//found " << unusedFunctions.size() << " functions to remove\n";

        // Remove all unused variables and functions.
        auto globalsEnd = unusedGlobals.end();
        for (auto && unusedGlobal = unusedGlobals.begin(); unusedGlobal != globalsEnd; ++unusedGlobal) {
          tu->removeDecl(*unusedGlobal);
        }

        for (FunctionDecl *unusedFn : unusedFunctions) {
          tu->removeDecl(unusedFn);
        }

        o << "// Rewrite unused globals result:\n";
        PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());
        p.Indentation = 1;
        tu->print(o, p);

        WriteSemanticDefines(compiler, pHelper, o);
      }
    }
  }

  // Flush and return results.
  raw_string_ostream_to_CoString(o, pResult);
  raw_string_ostream_to_CoString(w, pWarnings);

  if (compiler.getDiagnosticClient().getNumErrors() > 0)
    return E_FAIL;
  return S_OK;
}

static void RemoveStaticDecls(DeclContext &Ctx) {
  for (auto it = Ctx.decls_begin(); it != Ctx.decls_end(); ) {
    auto cur = it++;
    if (VarDecl *VD = dyn_cast<VarDecl>(*cur)) {
      if (VD->getStorageClass() == SC_Static || VD->isInAnonymousNamespace()) {
        Ctx.removeDecl(VD);
      }
    }
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(*cur)) {
      if (isa<CXXMethodDecl>(FD))
        continue;
      if (FD->getStorageClass() == SC_Static || FD->isInAnonymousNamespace()) {
        Ctx.removeDecl(FD);
      }
    }

    if (DeclContext *DC = dyn_cast<DeclContext>(*cur)) {
      RemoveStaticDecls(*DC);
    }
  }
}

static void GlobalVariableAsExternByDefault(DeclContext &Ctx) {
  for (auto it = Ctx.decls_begin(); it != Ctx.decls_end(); ) {
    auto cur = it++;
    if (VarDecl *VD = dyn_cast<VarDecl>(*cur)) {
      bool isInternal = VD->getStorageClass() == SC_Static || VD->isInAnonymousNamespace();
      if (!isInternal) {
        VD->setStorageClass(StorageClass::SC_Extern);
      }
    }
    // Only iterate on namespaces.
    if (NamespaceDecl *DC = dyn_cast<NamespaceDecl>(*cur)) {
      GlobalVariableAsExternByDefault(*DC);
    }
  }
}


static
HRESULT DoSimpleReWrite(_In_ DxcLangExtensionsHelper *pHelper,
               _In_ LPCSTR pFileName,
               _In_ ASTUnit::RemappedFile *pRemap,
               _In_ LPCSTR pDefines,
               _In_ UINT32 rewriteOption,
               _Outptr_result_z_ LPSTR *pWarnings,
               _Outptr_result_z_ LPSTR *pResult) {
  if (pWarnings != nullptr) *pWarnings = nullptr;
  if (pResult != nullptr) *pResult = nullptr;

  bool bSkipFunctionBody = rewriteOption & RewriterOptionMask::SkipFunctionBody;
  bool bSkipStatic = rewriteOption & RewriterOptionMask::SkipStatic;
  bool bGlobalExternByDefault = rewriteOption & RewriterOptionMask::GlobalExternByDefault;
  bool bKeepUserMacro = rewriteOption & RewriterOptionMask::KeepUserMacro;

  std::string s, warnings;
  raw_string_ostream o(s);
  raw_string_ostream w(warnings);

  // Setup a compiler instance.
  CompilerInstance compiler;
  std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
      llvm::make_unique<TextDiagnosticPrinter>(w, &compiler.getDiagnosticOpts());    
  SetupCompilerForRewrite(compiler, pHelper, pFileName, diagPrinter.get(), pRemap, pDefines);

  // Parse the source file.
  compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(), &compiler.getPreprocessor());

  ParseAST(compiler.getSema(), false, bSkipFunctionBody);

  ASTContext& C = compiler.getASTContext();
  TranslationUnitDecl *tu = C.getTranslationUnitDecl();

  if (bSkipStatic && bSkipFunctionBody) {
    // Remove static functions and globals.
    RemoveStaticDecls(*tu);
  }

  if (bGlobalExternByDefault) {
    GlobalVariableAsExternByDefault(*tu);
  }

  o << "// Rewrite unchanged result:\n";
  PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());
  p.Indentation = 1;
  tu->print(o, p);

  WriteSemanticDefines(compiler, pHelper, o);
  if (bKeepUserMacro)
    WriteUserMacroDefines(compiler, o);

  // Flush and return results.
  raw_string_ostream_to_CoString(o, pResult);
  raw_string_ostream_to_CoString(w, pWarnings);

  if (compiler.getDiagnosticClient().getNumErrors() > 0)
    return E_FAIL;
  return S_OK;
}

class DxcRewriter : public IDxcRewriter, public IDxcLangExtensions {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  DxcLangExtensionsHelper m_langExtensionsHelper;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcRewriter)
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langExtensionsHelper)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcRewriter, IDxcLangExtensions>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE RemoveUnusedGlobals(_In_ IDxcBlobEncoding *pSource,
                                                _In_z_ LPCWSTR pEntryPoint,
                                                _In_count_(defineCount) DxcDefine *pDefines,
                                                _In_ UINT32 defineCount,
                                                _COM_Outptr_ IDxcOperationResult **ppResult) override
  {
    
    if (pSource == nullptr || ppResult == nullptr || (defineCount > 0 && pDefines == nullptr))
      return E_INVALIDARG;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobEncoding> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem* msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data((LPSTR)utf8Source->GetBufferPointer(), utf8Source->GetBufferSize());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      CW2A utf8EntryPoint(pEntryPoint, CP_UTF8);
      std::string definesStr = DefinesToString(pDefines, defineCount);

      LPSTR errors = nullptr;
      LPSTR rewrite = nullptr;
      HRESULT status = DoRewriteUnused(
          &m_langExtensionsHelper, fakeName, pRemap.get(), utf8EntryPoint,
          defineCount > 0 ? definesStr.c_str() : nullptr, &errors, &rewrite);
      return DxcOperationResult::CreateFromUtf8Strings(errors, rewrite, status,
                                                       ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE 
  RewriteUnchanged(_In_ IDxcBlobEncoding *pSource,
                   _In_count_(defineCount) DxcDefine *pDefines,
                   _In_ UINT32 defineCount,
                   _COM_Outptr_ IDxcOperationResult **ppResult) override {
    if (pSource == nullptr || ppResult == nullptr || (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobEncoding> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem* msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data((LPCSTR)utf8Source->GetBufferPointer(), utf8Source->GetBufferSize());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      std::string definesStr = DefinesToString(pDefines, defineCount);

      LPSTR errors = nullptr;
      LPSTR rewrite = nullptr;
      HRESULT status =
          DoSimpleReWrite(&m_langExtensionsHelper, fakeName, pRemap.get(),
                          defineCount > 0 ? definesStr.c_str() : nullptr,
                          RewriterOptionMask::Default, &errors, &rewrite);

      return DxcOperationResult::CreateFromUtf8Strings(errors, rewrite, status,
                                                       ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();

  }

  HRESULT STDMETHODCALLTYPE RewriteUnchangedWithInclude(
      _In_ IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      _In_opt_ LPCWSTR pSourceName, _In_count_(defineCount) DxcDefine *pDefines,
      _In_ UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      _In_opt_ IDxcIncludeHandler *pIncludeHandler,
      _In_ UINT32 rewriteOption,
      _COM_Outptr_ IDxcOperationResult **ppResult) override {
    if (pSource == nullptr || ppResult == nullptr || (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobEncoding> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, &utf8Source));

    CW2A utf8SourceName(pSourceName, CP_UTF8);
    LPCSTR fName = utf8SourceName.m_psz;

    try {
      dxcutil::DxcArgsFileSystem *msfPtr = dxcutil::CreateDxcArgsFileSystem(utf8Source, pSourceName, pIncludeHandler);
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data((LPCSTR)utf8Source->GetBufferPointer(), utf8Source->GetBufferSize());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fName, pBuffer.release()));

      std::string definesStr = DefinesToString(pDefines, defineCount);

      LPSTR errors = nullptr;
      LPSTR rewrite = nullptr;
      HRESULT status =
          DoSimpleReWrite(&m_langExtensionsHelper, fName, pRemap.get(),
                          defineCount > 0 ? definesStr.c_str() : nullptr,
                          rewriteOption, &errors, &rewrite);

      return DxcOperationResult::CreateFromUtf8Strings(errors, rewrite, status,
                                                       ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();

  }

  std::string DefinesToString(_In_count_(defineCount) DxcDefine *pDefines, _In_ UINT32 defineCount) {
    std::string defineStr;
    for (UINT32 i = 0; i < defineCount; i++) {
      CW2A utf8Name(pDefines[i].Name, CP_UTF8);
      CW2A utf8Value(pDefines[i].Value, CP_UTF8);
      defineStr += "#define ";
      defineStr += utf8Name;
      defineStr += " ";
      defineStr += utf8Value ? utf8Value.m_psz : "1";
      defineStr += "\n";
    }

    return defineStr;
  }
};

HRESULT CreateDxcRewriter(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcRewriter> isense = DxcRewriter::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(isense.p);
  return isense.p->QueryInterface(riid, ppv);
}
