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
#include "clang/Frontend/FrontendActions.h"
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
#include "dxc/Support/HLSLOptions.h"

// From dxcutil.h
namespace dxcutil {
bool IsAbsoluteOrCurDirRelative(const llvm::Twine &T);
} // namespace dxcutil

#define CP_UTF16 1200

using namespace llvm;
using namespace clang;
using namespace hlsl;

struct RewriteHelper {
  SmallPtrSet<VarDecl *, 128> unusedGlobals;
  SmallPtrSet<FunctionDecl *, 128> unusedFunctions;
  SmallPtrSet<TypeDecl *, 32> unusedTypes;
  DenseMap<RecordDecl *, unsigned> anonymousRecordRefCounts;
};

struct ASTHelper {
  CompilerInstance compiler;
  TranslationUnitDecl *tu;
  ParsedSemanticDefineList semanticMacros;
  ParsedSemanticDefineList userMacros;
  bool bHasErrors;
};

static FunctionDecl *getFunctionWithBody(FunctionDecl *F) {
  if (!F)
    return nullptr;
  if (F->doesThisDeclarationHaveABody())
    return F;
  F = F->getFirstDecl();
  for (auto &&Candidate : F->redecls()) {
    if (Candidate->doesThisDeclarationHaveABody()) {
      return Candidate;
    }
  }
  return nullptr;
}

static void SaveTypeDecl(TagDecl *tagDecl,
                          SmallPtrSetImpl<TypeDecl *> &visitedTypes) {
  if (visitedTypes.count(tagDecl))
    return;
  visitedTypes.insert(tagDecl);
  if (CXXRecordDecl *recordDecl = dyn_cast<CXXRecordDecl>(tagDecl)) {
    // If template, save template args
    if (const ClassTemplateSpecializationDecl *templateSpecializationDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {
      const clang::TemplateArgumentList &args =
          templateSpecializationDecl->getTemplateInstantiationArgs();
      for (unsigned i = 0; i < args.size(); ++i) {
        const clang::TemplateArgument &arg = args[i];
        switch (arg.getKind()) {
        case clang::TemplateArgument::ArgKind::Type:
          if (TagDecl *tagDecl = arg.getAsType()->getAsTagDecl()) {
            SaveTypeDecl(tagDecl, visitedTypes);
          };
          break;
        default:
          break;
        }
      }
    }
    // Add field types.
    for (FieldDecl *fieldDecl : recordDecl->fields()) {
      if (TagDecl *tagDecl = fieldDecl->getType()->getAsTagDecl()) {
        SaveTypeDecl(tagDecl, visitedTypes);
      }
    }
    // Add base types.
    if (recordDecl->getNumBases()) {
      for (auto &I : recordDecl->bases()) {
        CXXRecordDecl *BaseDecl =
            cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
        SaveTypeDecl(BaseDecl, visitedTypes);
      }
    }
  }
}

class VarReferenceVisitor : public RecursiveASTVisitor<VarReferenceVisitor> {
private:
  SmallPtrSetImpl<VarDecl*>& m_unusedGlobals;
  SmallPtrSetImpl<FunctionDecl*>& m_visitedFunctions;
  SmallVectorImpl<FunctionDecl*>& m_pendingFunctions;
  SmallPtrSetImpl<TypeDecl *> &m_visitedTypes;

  void AddRecordType(TagDecl *tagDecl) {
    SaveTypeDecl(tagDecl, m_visitedTypes);
  }

public:
  VarReferenceVisitor(
    SmallPtrSetImpl<VarDecl*>& unusedGlobals,
    SmallPtrSetImpl<FunctionDecl*>& visitedFunctions,
    SmallVectorImpl<FunctionDecl*>& pendingFunctions,
    SmallPtrSetImpl<TypeDecl *> &types)  :
    m_unusedGlobals(unusedGlobals),
    m_visitedFunctions(visitedFunctions),
    m_pendingFunctions(pendingFunctions),
    m_visitedTypes(types) {
  }

  bool VisitDeclRefExpr(DeclRefExpr* ref) {
    ValueDecl* valueDecl = ref->getDecl();
    if (FunctionDecl* fnDecl = dyn_cast_or_null<FunctionDecl>(valueDecl)) {
      FunctionDecl *fnDeclWithbody = getFunctionWithBody(fnDecl);
      if (fnDeclWithbody) {
        if (!m_visitedFunctions.count(fnDeclWithbody)) {
          m_pendingFunctions.push_back(fnDeclWithbody);
        }
      }
      if (fnDeclWithbody && fnDeclWithbody != fnDecl) {
        // In case fnDecl is only a decl, setDecl to fnDeclWithbody.
        ref->setDecl(fnDeclWithbody);
        // Keep the fnDecl for now, since it might be predecl.
        m_visitedFunctions.insert(fnDecl);
      }
    }
    else if (VarDecl* varDecl = dyn_cast_or_null<VarDecl>(valueDecl)) {
      m_unusedGlobals.erase(varDecl);
      if (TagDecl *tagDecl = varDecl->getType()->getAsTagDecl()) {
        AddRecordType(tagDecl);
      }
      if (Expr *initExp = varDecl->getInit()) {
        if (InitListExpr *initList =
                dyn_cast<InitListExpr>(initExp)) {
          TraverseInitListExpr(initList);
        } else if (ImplicitCastExpr *initCast = dyn_cast<ImplicitCastExpr>(initExp)) {
          TraverseImplicitCastExpr(initCast);
        } else if (DeclRefExpr *initRef = dyn_cast<DeclRefExpr>(initExp)) {
          TraverseDeclRefExpr(initRef);
        }
      }
    }
    return true;
  }
  bool VisitMemberExpr(MemberExpr *expr) {
    // Save nested struct type.
    if (TagDecl *tagDecl = expr->getType()->getAsTagDecl()) {
      m_visitedTypes.insert(tagDecl);
    }
    return true;
  }
  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *expr) {
    if (FunctionDecl *fnDecl =
            dyn_cast_or_null<FunctionDecl>(expr->getCalleeDecl())) {
      if (!m_visitedFunctions.count(fnDecl)) {
        m_pendingFunctions.push_back(fnDecl);
      }
    }
    if (CXXRecordDecl *recordDecl = expr->getRecordDecl()) {
      AddRecordType(recordDecl);
    }
    return true;
  }
  bool VisitHLSLBufferDecl(HLSLBufferDecl *bufDecl) {
    if (!bufDecl->isCBuffer())
      return false;
    for (Decl *decl : bufDecl->decls()) {
      if (VarDecl *constDecl = dyn_cast<VarDecl>(decl)) {
        if (TagDecl *tagDecl = constDecl->getType()->getAsTagDecl()) {
          AddRecordType(tagDecl);
        }
      } else if (isa<EmptyDecl>(decl)) {
        // Nothing to do for this declaration.
      } else if (CXXRecordDecl *recordDecl = dyn_cast<CXXRecordDecl>(decl)) {
        m_visitedTypes.insert(recordDecl);
      } else if (isa<FunctionDecl>(decl)) {
        // A function within an cbuffer is effectively a top-level function,
        // as it only refers to globally scoped declarations.
        // Nothing to do for this declaration.
      } else {
        HLSLBufferDecl *inner = cast<HLSLBufferDecl>(decl);
        VisitHLSLBufferDecl(inner);
      }
    }
    return true;
  }
};

// Macro related.
namespace {

bool MacroPairCompareIsLessThan(
    const std::pair<const IdentifierInfo *, const MacroInfo *> &left,
    const std::pair<const IdentifierInfo *, const MacroInfo *> &right) {
  return left.first->getName().compare(right.first->getName()) < 0;
}

ParsedSemanticDefineList
CollectUserMacrosParsedByCompiler(CompilerInstance &compiler) {
  ParsedSemanticDefineList parsedDefines;
  // This is very inefficient in general, but in practice we either have
  // no semantic defines, or we have a star define for a some reserved prefix.
  // These will be sorted so rewrites are stable.
  std::vector<std::pair<const IdentifierInfo *, MacroInfo *>> macros;
  Preprocessor &pp = compiler.getPreprocessor();
  Preprocessor::macro_iterator end = pp.macro_end();
  SourceManager &SM = compiler.getSourceManager();
  FileID PredefineFileID = pp.getPredefinesFileID();

  for (Preprocessor::macro_iterator i = pp.macro_begin(); i != end; ++i) {
    if (!i->second.getLatest()->isDefined()) {
      continue;
    }
    MacroInfo *mi = i->second.getLatest()->getMacroInfo();
    if (mi->getDefinitionLoc().isInvalid()) {
      continue;
    }
    FileID FID = SM.getFileID(mi->getDefinitionEndLoc());
    if (FID == PredefineFileID)
      continue;

    const IdentifierInfo *ii = i->first;

    macros.push_back(std::pair<const IdentifierInfo *, MacroInfo *>(ii, mi));
  }

  if (!macros.empty()) {
    std::sort(macros.begin(), macros.end(), MacroPairCompareIsLessThan);
    MacroExpander expander(pp);
    for (std::pair<const IdentifierInfo *, MacroInfo *> m : macros) {
      std::string expandedValue;
      MacroInfo *mi = m.second;
      if (!mi->isFunctionLike()) {
        expander.ExpandMacro(m.second, &expandedValue);
        parsedDefines.emplace_back(ParsedSemanticDefine{
            m.first->getName(), expandedValue,
            m.second->getDefinitionLoc().getRawEncoding()});
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
        parsedDefines.emplace_back(ParsedSemanticDefine{
            macroStr, macroValStr,
            m.second->getDefinitionLoc().getRawEncoding()});
      }
    }
  }

  return parsedDefines;
}

bool IsMacroMatch(StringRef name, const std::string &mask) {
  return Unicode::IsStarMatchUTF8(mask.c_str(), mask.size(), name.data(),
                                  name.size());
}

void WriteMacroDefines(ParsedSemanticDefineList &macros,
                       raw_string_ostream &o) {
  if (!macros.empty()) {
    o << "\n// Macros:\n";
    for (auto &&m : macros) {
      o << "#define " << m.Name << " " << m.Value << "\n";
    }
  }
}

} // namespace

ParsedSemanticDefineList hlsl::CollectSemanticDefinesParsedByCompiler(
    CompilerInstance &compiler, _In_ DxcLangExtensionsHelper *helper) {
  ParsedSemanticDefineList parsedDefines;
  const llvm::SmallVector<std::string, 2> &defines =
      helper->GetSemanticDefines();
  if (defines.size() == 0) {
    return parsedDefines;
  }

  const llvm::SmallVector<std::string, 2> &defineExclusions =
      helper->GetSemanticDefineExclusions();

  // This is very inefficient in general, but in practice we either have
  // no semantic defines, or we have a star define for a some reserved prefix.
  // These will be sorted so rewrites are stable.
  std::vector<std::pair<const IdentifierInfo *, MacroInfo *>> macros;
  Preprocessor &pp = compiler.getPreprocessor();
  Preprocessor::macro_iterator end = pp.macro_end();
  for (Preprocessor::macro_iterator i = pp.macro_begin(); i != end; ++i) {
    if (!i->second.getLatest()->isDefined()) {
      continue;
    }
    MacroInfo *mi = i->second.getLatest()->getMacroInfo();
    if (mi->isFunctionLike()) {
      continue;
    }

    const IdentifierInfo *ii = i->first;

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

      macros.push_back(std::pair<const IdentifierInfo *, MacroInfo *>(ii, mi));
    }
  }

  if (!macros.empty()) {
    std::sort(macros.begin(), macros.end(), MacroPairCompareIsLessThan);
    MacroExpander expander(pp);
    for (std::pair<const IdentifierInfo *, MacroInfo *> m : macros) {
      std::string expandedValue;
      expander.ExpandMacro(m.second, &expandedValue);
      parsedDefines.emplace_back(
          ParsedSemanticDefine{m.first->getName(), expandedValue,
                               m.second->getDefinitionLoc().getRawEncoding()});
    }
  }

  return parsedDefines;
}


namespace {

void SetupCompilerCommon(CompilerInstance &compiler,
                         _In_ DxcLangExtensionsHelper *helper,
                         _In_ LPCSTR pMainFile,
                         _In_ TextDiagnosticPrinter *diagPrinter,
                         _In_opt_ ASTUnit::RemappedFile *rewrite,
                         _In_ hlsl::options::DxcOpts &opts,
                         _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr) {
  // Setup a compiler instance.
  std::shared_ptr<TargetOptions> targetOptions(new TargetOptions);
  targetOptions->Triple = llvm::sys::getDefaultTargetTriple();
  compiler.HlslLangExtensions = helper;
  compiler.createDiagnostics(diagPrinter, false);
  compiler.createFileManager();
  compiler.createSourceManager(compiler.getFileManager());
  compiler.setTarget(
      TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), targetOptions));
  // Not use builtin includes.
  compiler.getHeaderSearchOpts().UseBuiltinIncludes = false;

  // apply compiler options applicable for rewrite
  if (opts.WarningAsError)
    compiler.getDiagnostics().setWarningsAsErrors(true);
  compiler.getDiagnostics().setIgnoreAllWarnings(!opts.OutputWarnings);
  compiler.getLangOpts().HLSLVersion = (unsigned)opts.HLSLVersion;
  compiler.getLangOpts().UseMinPrecision = !opts.Enable16BitTypes;
  compiler.getLangOpts().EnableDX9CompatMode = opts.EnableDX9CompatMode;
  compiler.getLangOpts().EnableFXCCompatMode = opts.EnableFXCCompatMode;
  compiler.getDiagnostics().setIgnoreAllWarnings(!opts.OutputWarnings);
  compiler.getCodeGenOpts().MainFileName = pMainFile;

  PreprocessorOptions &PPOpts = compiler.getPreprocessorOpts();
  if (rewrite != nullptr) {
    if (llvm::MemoryBuffer *pMemBuf = rewrite->second) {
      compiler.getPreprocessorOpts().addRemappedFile(StringRef(pMainFile),
                                                     pMemBuf);
    }

    PPOpts.RemappedFilesKeepOriginalName = true;
  }

  PPOpts.ExpandTokPastingArg = opts.LegacyMacroExpansion;

  // Pick additional arguments.
  clang::HeaderSearchOptions &HSOpts = compiler.getHeaderSearchOpts();
  HSOpts.UseBuiltinIncludes = 0;
  // Consider: should we force-include '.' if the source file is relative?
  for (const llvm::opt::Arg *A : opts.Args.filtered(options::OPT_I)) {
    const bool IsFrameworkFalse = false;
    const bool IgnoreSysRoot = true;
    if (dxcutil::IsAbsoluteOrCurDirRelative(A->getValue())) {
      HSOpts.AddPath(A->getValue(), frontend::Angled, IsFrameworkFalse,
                     IgnoreSysRoot);
    } else {
      std::string s("./");
      s += A->getValue();
      HSOpts.AddPath(s, frontend::Angled, IsFrameworkFalse, IgnoreSysRoot);
    }
  }

}

void SetupCompilerForRewrite(
    CompilerInstance &compiler, _In_ DxcLangExtensionsHelper *helper,
    _In_ LPCSTR pMainFile, _In_ TextDiagnosticPrinter *diagPrinter,
    _In_opt_ ASTUnit::RemappedFile *rewrite, _In_ hlsl::options::DxcOpts &opts,
    _In_opt_ LPCSTR pDefines, _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr) {

  SetupCompilerCommon(compiler, helper, pMainFile, diagPrinter, rewrite, opts,
                      msfPtr);

  if (msfPtr) {
    msfPtr->SetupForCompilerInstance(compiler);
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

  const FileEntry *mainFileEntry =
      compiler.getFileManager().getFile(StringRef(pMainFile));
  if (mainFileEntry == nullptr) {
    throw ::hlsl::Exception(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
  }
  compiler.getSourceManager().setMainFileID(
      compiler.getSourceManager().createFileID(mainFileEntry, SourceLocation(),
                                               SrcMgr::C_User));
}


void SetupCompilerForPreprocess(
    CompilerInstance &compiler, _In_ DxcLangExtensionsHelper *helper,
    _In_ LPCSTR pMainFile, _In_ TextDiagnosticPrinter *diagPrinter,
    _In_opt_ ASTUnit::RemappedFile *rewrite, _In_ hlsl::options::DxcOpts &opts,
    _In_ DxcDefine *pDefines, _In_ UINT32 defineCount,
    _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr) {

  SetupCompilerCommon(compiler, helper, pMainFile, diagPrinter, rewrite, opts,
                      msfPtr);

  if (pDefines) {
    PreprocessorOptions &PPOpts = compiler.getPreprocessorOpts();
    for (size_t i = 0; i < defineCount; ++i) {
      CW2A utf8Name(pDefines[i].Name, CP_UTF8);
      CW2A utf8Value(pDefines[i].Value, CP_UTF8);
      std::string val(utf8Name.m_psz);
      val += "=";
      val += (pDefines[i].Value) ? utf8Value.m_psz : "1";
      PPOpts.addMacroDef(val);
    }
  }
}


std::string DefinesToString(_In_count_(defineCount) DxcDefine *pDefines,
                            _In_ UINT32 defineCount) {
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

HRESULT GenerateAST(DxcLangExtensionsHelper *pExtHelper, LPCSTR pFileName,
                    ASTUnit::RemappedFile *pRemap, DxcDefine *pDefines,
                    UINT32 defineCount, ASTHelper &astHelper,
                    hlsl::options::DxcOpts &opts,
                    dxcutil::DxcArgsFileSystem *msfPtr, raw_ostream &w) {
  // Setup a compiler instance.
  CompilerInstance &compiler = astHelper.compiler;
  std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
      llvm::make_unique<TextDiagnosticPrinter>(w,
                                               &compiler.getDiagnosticOpts());
  std::string definesStr = DefinesToString(pDefines, defineCount);

  SetupCompilerForRewrite(
      compiler, pExtHelper, pFileName, diagPrinter.get(), pRemap, opts,
      defineCount > 0 ? definesStr.c_str() : nullptr, msfPtr);

  // Parse the source file.
  compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),
                                                 &compiler.getPreprocessor());

  ParseAST(compiler.getSema(), false, opts.RWOpt.SkipFunctionBody);

  ASTContext &C = compiler.getASTContext();
  TranslationUnitDecl *tu = C.getTranslationUnitDecl();
  astHelper.tu = tu;

  if (compiler.getDiagnosticClient().getNumErrors() > 0) {
    astHelper.bHasErrors = true;
    w.flush();
    return E_FAIL;
  }
  astHelper.bHasErrors = false;

  astHelper.semanticMacros =
      CollectSemanticDefinesParsedByCompiler(compiler, pExtHelper);

  if (opts.RWOpt.KeepUserMacro)
    astHelper.userMacros = CollectUserMacrosParsedByCompiler(compiler);
  return S_OK;
}

HRESULT CollectRewriteHelper(TranslationUnitDecl *tu, LPCSTR pEntryPoint,
                             RewriteHelper &helper, bool bRemoveGlobals,
                             bool bRemoveFunctions, raw_ostream &w) {
  ASTContext &C = tu->getASTContext();

  // Gather all global variables that are not in cbuffers and all functions.
  SmallPtrSet<VarDecl *, 128> &unusedGlobals = helper.unusedGlobals;
  DenseMap<RecordDecl *, unsigned> &anonymousRecordRefCounts =
      helper.anonymousRecordRefCounts;
  SmallPtrSet<FunctionDecl *, 128> &unusedFunctions = helper.unusedFunctions;
  SmallPtrSet<TypeDecl *, 32> &unusedTypes = helper.unusedTypes;
  SmallVector<VarDecl *, 32> nonStaticGlobals;
  SmallVector<HLSLBufferDecl *, 16> cbufferDecls;
  for (Decl *tuDecl : tu->decls()) {
    if (tuDecl->isImplicit())
      continue;

    VarDecl *varDecl = dyn_cast_or_null<VarDecl>(tuDecl);
    if (varDecl != nullptr) {
      if (!bRemoveGlobals) {
        // Only remove static global when not remove global.
        if (!(varDecl->getStorageClass() == SC_Static ||
              varDecl->isInAnonymousNamespace())) {
          nonStaticGlobals.emplace_back(varDecl);
          continue;
        }
      }

      unusedGlobals.insert(varDecl);
      if (const RecordType *recordType =
              varDecl->getType()->getAs<RecordType>()) {
        RecordDecl *recordDecl = recordType->getDecl();
        if (recordDecl && recordDecl->getName().empty()) {
          anonymousRecordRefCounts[recordDecl]++; // Zero initialized if
                                                  // non-existing
        }
      }
      continue;
    }

    if (HLSLBufferDecl *CB = dyn_cast<HLSLBufferDecl>(tuDecl)) {
      if (!CB->isCBuffer())
        continue;
      cbufferDecls.emplace_back(CB);
      continue;
    }

    FunctionDecl *fnDecl = dyn_cast_or_null<FunctionDecl>(tuDecl);
    if (fnDecl != nullptr) {
      FunctionDecl *fnDeclWithbody = getFunctionWithBody(fnDecl);
      // Add fnDecl without body which has a define somewhere.
      if (fnDecl->doesThisDeclarationHaveABody() || fnDeclWithbody) {
        unusedFunctions.insert(fnDecl);
      }
    }

    if (TagDecl *tagDecl = dyn_cast<TagDecl>(tuDecl)) {
      unusedTypes.insert(tagDecl);
      if (CXXRecordDecl *recordDecl = dyn_cast<CXXRecordDecl>(tagDecl)) {
        for (CXXMethodDecl *methodDecl : recordDecl->methods()) {
          unusedFunctions.insert(methodDecl);
        }
      }
    }
  }

  w << "//found " << unusedGlobals.size()
    << " globals as candidates for removal\n";
  w << "//found " << unusedFunctions.size()
    << " functions as candidates for removal\n";

  DeclContext::lookup_result l =
      tu->lookup(DeclarationName(&C.Idents.get(StringRef(pEntryPoint))));
  if (l.empty()) {
    w << "//entry point not found\n";
    return E_FAIL;
  }

  w << "//entry point found\n";
  NamedDecl *entryDecl = l.front();
  FunctionDecl *entryFnDecl = dyn_cast_or_null<FunctionDecl>(entryDecl);
  if (entryFnDecl == nullptr) {
    w << "//entry point found but is not a function declaration\n";
    return E_FAIL;
  }

  // Traverse reachable functions and variables.
  SmallPtrSet<FunctionDecl *, 128> visitedFunctions;
  SmallVector<FunctionDecl *, 32> pendingFunctions;
  SmallPtrSet<TypeDecl *, 32> visitedTypes;
  VarReferenceVisitor visitor(unusedGlobals, visitedFunctions, pendingFunctions,
                              visitedTypes);
  pendingFunctions.push_back(entryFnDecl);
  while (!pendingFunctions.empty()) {
    FunctionDecl *pendingDecl = pendingFunctions.pop_back_val();
    visitedFunctions.insert(pendingDecl);
    visitor.TraverseDecl(pendingDecl);
  }
  // Traverse cbuffers to save types for cbuffer constant.
  for (auto *CBDecl : cbufferDecls) {
    visitor.TraverseDecl(CBDecl);
  }

  // Don't bother doing work if there are no globals to remove.
  if (unusedGlobals.empty() && unusedFunctions.empty() && unusedTypes.empty()) {
    return S_FALSE;
  }

  w << "//found " << unusedGlobals.size() << " globals to remove\n";

  // Don't remove visited functions.
  for (FunctionDecl *visitedFn : visitedFunctions) {
    unusedFunctions.erase(visitedFn);
  }
  w << "//found " << unusedFunctions.size() << " functions to remove\n";

  for (VarDecl *varDecl : nonStaticGlobals) {
    if (TagDecl *tagDecl = varDecl->getType()->getAsTagDecl()) {
      SaveTypeDecl(tagDecl, visitedTypes);
    }
  }
  for (TypeDecl *typeDecl : visitedTypes) {
    unusedTypes.erase(typeDecl);
  }

  w << "//found " << unusedTypes.size() << " types to remove\n";
  return S_OK;
}

} // namespace

static
HRESULT ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                            hlsl::options::DxcOpts &opts,
                            _COM_Outptr_ IDxcOperationResult **ppResult) {
  const llvm::opt::OptTable *table = ::options::getHlslOptTable();

  CComPtr<AbstractMemoryStream> pOutputStream;
  IFT(CreateMemoryStream(GetGlobalHeapMalloc(), &pOutputStream));
  raw_stream_ostream outStream(pOutputStream);

  if (0 != hlsl::options::ReadDxcOpts(table, hlsl::options::HlslFlags::RewriteOption,
                                      mainArgs, opts, outStream)) {
    CComPtr<IDxcBlob> pErrorBlob;
    IFT(pOutputStream->QueryInterface(&pErrorBlob));
    outStream.flush();
    IFT(DxcResult::Create(E_INVALIDARG, DXC_OUT_NONE, {
        DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
          (LPCSTR)pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize())
      }, ppResult));
    return S_OK;
  }
  return S_OK;
}

static
bool HasUniformParams(FunctionDecl *FD) {
  for (auto PD : FD->params()) {
    if (PD->hasAttr<HLSLUniformAttr>())
      return true;
  }
  return false;
}

static
void WriteUniformParamsAsGlobals(FunctionDecl *FD,
                                 raw_ostream &o,
                                 PrintingPolicy &p) {
  // Extract resources first, to avoid placing in cbuffer _Params
  for (auto PD : FD->params()) {
    if (PD->hasAttr<HLSLUniformAttr>() &&
        hlsl::IsHLSLResourceType(PD->getType())) {
      PD->print(o, p);
      o << ";\n";
    }
  }
  // Extract any non-resource uniforms into cbuffer _Params
  bool startedParams = false;
  for (auto PD : FD->params()) {
    if (PD->hasAttr<HLSLUniformAttr>() &&
        !hlsl::IsHLSLResourceType(PD->getType())) {
      if (!startedParams) {
        o << "cbuffer _Params {\n";
        startedParams = true;
      }
      PD->print(o, p);
      o << ";\n";
    }
  }
  if (startedParams) {
    o << "}\n";
  }
}

static
void PrintTranslationUnitWithTranslatedUniformParams(
    TranslationUnitDecl *tu,
    FunctionDecl *entryFnDecl,
    raw_ostream &o,
    PrintingPolicy &p) {
  // Print without the entry function
  entryFnDecl->setImplicit(true); // Prevent printing of this decl
  tu->print(o, p);
  entryFnDecl->setImplicit(false);

  WriteUniformParamsAsGlobals(entryFnDecl, o, p);

  PrintingPolicy SubPolicy(p);
  SubPolicy.HLSLSuppressUniformParameters = true;
  entryFnDecl->print(o, SubPolicy);
}

static HRESULT DoRewriteUnused( TranslationUnitDecl *tu,
                                LPCSTR pEntryPoint,
                                bool bRemoveGlobals,
                                bool bRemoveFunctions,
                                raw_ostream &w) {
  RewriteHelper helper;
  HRESULT hr = CollectRewriteHelper(tu, pEntryPoint, helper, bRemoveGlobals,
                                    bRemoveFunctions, w);
  if (hr != S_OK)
    return hr;

  // Remove all unused variables and functions.
  for (VarDecl *unusedGlobal : helper.unusedGlobals) {
    if (const RecordType *recordTy = unusedGlobal->getType()->getAs<RecordType>()) {
      RecordDecl *recordDecl = recordTy->getDecl();
      if (recordDecl && recordDecl->getName().empty()) {
        // Anonymous structs can only be referenced by the variable they declare.
        // If we've removed all declared variables of such a struct, remove it too,
        // because anonymous structs without variable declarations in global scope are illegal.
        auto recordRefCountIter = helper.anonymousRecordRefCounts.find(recordDecl);
        DXASSERT_NOMSG(recordRefCountIter != helper.anonymousRecordRefCounts.end() && recordRefCountIter->second > 0);
        recordRefCountIter->second--;
        if (recordRefCountIter->second == 0) {
          tu->removeDecl(recordDecl);
          helper.anonymousRecordRefCounts.erase(recordRefCountIter);
        }
      }
    }
    if (HLSLBufferDecl *CBV = dyn_cast<HLSLBufferDecl>(unusedGlobal->getLexicalDeclContext())) {
      if (CBV->isConstantBufferView()) {
        // For constant buffer view, we create a variable for the constant.
        // The variable use tu as the DeclContext to access as global variable, CBV as LexicalDeclContext so it is still part of CBV.
        // setLexicalDeclContext to tu to avoid assert when remove.
        unusedGlobal->setLexicalDeclContext(tu);
      }
    }
    tu->removeDecl(unusedGlobal);
  }

  for (FunctionDecl *unusedFn : helper.unusedFunctions) {
    // remove name of function to workaround assert when update lookup table.
    unusedFn->setDeclName(DeclarationName());
    if (CXXMethodDecl *methodDecl = dyn_cast<CXXMethodDecl>(unusedFn)) {
      methodDecl->getParent()->removeDecl(unusedFn);
    } else {
      tu->removeDecl(unusedFn);
    }
  }

  for (TypeDecl *unusedTy : helper.unusedTypes) {
    tu->removeDecl(unusedTy);
  }
  // Flush and return results.
  w.flush();
  return S_OK;
}

static HRESULT
DoRewriteUnused(_In_ DxcLangExtensionsHelper *pHelper, _In_ LPCSTR pFileName,
                _In_ ASTUnit::RemappedFile *pRemap, _In_ LPCSTR pEntryPoint,
                _In_ DxcDefine *pDefines, _In_ UINT32 defineCount,
                bool bRemoveGlobals, bool bRemoveFunctions,
                std::string &warnings, std::string &result,
                _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr) {

  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;
  hlsl::options::DxcOpts opts;
  opts.HLSLVersion = 2015;

  GenerateAST(pHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
              opts, msfPtr, w);

  if (astHelper.bHasErrors)
    return E_FAIL;

  TranslationUnitDecl *tu = astHelper.tu;
  HRESULT hr =
      DoRewriteUnused(tu, pEntryPoint, bRemoveGlobals, bRemoveFunctions, w);
  if (FAILED(hr))
    return hr;

  ASTContext &C = tu->getASTContext();
  if (hr == S_FALSE) {
    w << "//no unused globals found - no work to be done\n";
    StringRef contents = C.getSourceManager().getBufferData(
        C.getSourceManager().getMainFileID());
    o << contents;
  } else {
    PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());
    p.Indentation = 1;
    tu->print(o, p);
  }

  WriteMacroDefines(astHelper.semanticMacros, o);

  // Flush and return results.
  o.flush();
  w.flush();

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
                _In_ hlsl::options::DxcOpts &opts, _In_ DxcDefine *pDefines,
                _In_ UINT32 defineCount,
               std::string &warnings,
               std::string &result,
               _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr) {
  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;

  GenerateAST(pHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
              opts, msfPtr, w);

  TranslationUnitDecl *tu = astHelper.tu;

  if (opts.RWOpt.SkipStatic && opts.RWOpt.SkipFunctionBody) {
    // Remove static functions and globals.
    RemoveStaticDecls(*tu);
  }

  if (opts.RWOpt.GlobalExternByDefault) {
    GlobalVariableAsExternByDefault(*tu);
  }

  if (opts.EntryPoint.empty())
    opts.EntryPoint = "main";

  if (opts.RWOpt.RemoveUnusedGlobals || opts.RWOpt.RemoveUnusedFunctions) {
    HRESULT hr = DoRewriteUnused(tu, opts.EntryPoint.data(),
                                 opts.RWOpt.RemoveUnusedGlobals,
                                     opts.RWOpt.RemoveUnusedFunctions, w);
    if (FAILED(hr))
      return hr;
  } else {
    o << "// Rewrite unchanged result:\n";
  }

  ASTContext &C = tu->getASTContext();

  FunctionDecl *entryFnDecl = nullptr;
  if (opts.RWOpt.ExtractEntryUniforms) {
    DeclContext::lookup_result l = tu->lookup(DeclarationName(&C.Idents.get(opts.EntryPoint)));
    if (l.empty()) {
      w << "//entry point not found\n";
      return E_FAIL;
    }
    entryFnDecl = dyn_cast_or_null<FunctionDecl>(l.front());
    if (!HasUniformParams(entryFnDecl))
      entryFnDecl = nullptr;
  }

  PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());
  p.Indentation = 1;

  if (entryFnDecl) {
    PrintTranslationUnitWithTranslatedUniformParams(tu, entryFnDecl, o, p);
  } else {
    tu->print(o, p);
  }

  WriteMacroDefines(astHelper.semanticMacros, o);
  if (opts.RWOpt.KeepUserMacro)
    WriteMacroDefines(astHelper.userMacros, o);

  // Flush and return results.
  o.flush();
  w.flush();

  if (astHelper.bHasErrors)
    return E_FAIL;
  return S_OK;
}

namespace {

void PreprocessResult(CompilerInstance &compiler, LPCSTR pFileName) {
  // These settings are back-compatible with fxc.
  clang::PreprocessorOutputOptions &PPOutOpts =
      compiler.getPreprocessorOutputOpts();
  PPOutOpts.ShowCPP = 1;           // Print normal preprocessed output.
  PPOutOpts.ShowComments = 0;      // Show comments.
  PPOutOpts.ShowLineMarkers = 1;   // Show \#line markers.
  PPOutOpts.UseLineDirectives = 1; // Use \#line instead of GCC-style \# N.
  PPOutOpts.ShowMacroComments = 0; // Show comments, even in macros.
  PPOutOpts.ShowMacros = 0;        // Print macro definitions.
  PPOutOpts.RewriteIncludes = 0;   // Preprocess include directives only.

  FrontendInputFile file(pFileName, IK_HLSL);
  clang::PrintPreprocessedAction action;
  if (action.BeginSourceFile(compiler, file)) {
    action.Execute();
    action.EndSourceFile();
  }
}

class RewriteVisitor : public RecursiveASTVisitor<RewriteVisitor> {
public:
  RewriteVisitor(Rewriter &R, TranslationUnitDecl *tu, RewriteHelper &helper)
      : TheRewriter(R), SourceMgr(R.getSourceMgr()), tu(tu),
        helper(helper), bNeedLineInfo(false) {}

  bool VisitFunctionDecl(FunctionDecl *f) {
    if (helper.unusedFunctions.count(f)) {
      bNeedLineInfo = true;

      TheRewriter.RemoveText(f->getSourceRange());
      return true;
    }

    AddLineInfoIfNeed(f->getLocStart());
    return true;
  }

  bool VisitTypeDecl(TypeDecl *t) {
    if (helper.unusedTypes.count(t)) {
      bNeedLineInfo = true;
      TheRewriter.RemoveText(t->getSourceRange());
      return true;
    }
    AddLineInfoIfNeed(t->getLocStart());
    return true;
  }

  bool VisitVarDecl(VarDecl *vd) {
    if (vd->getDeclContext() == tu) {
      if (helper.unusedGlobals.count(vd)) {
        bNeedLineInfo = true;
        TheRewriter.RemoveText(vd->getSourceRange());
        return true;
      }

      AddLineInfoIfNeed(vd->getLocStart());
    }
    return true;
  }

private:
  void AddLineInfoIfNeed(SourceLocation Loc) {
    if (bNeedLineInfo) {
      bNeedLineInfo = false;
      auto lineStr = MakeLineInfo(Loc);
      TheRewriter.InsertTextBefore(Loc, lineStr);
    }
  }
  std::string MakeLineInfo(SourceLocation Loc) {
    if (Loc.isInvalid())
      return "";
    if (!Loc.isFileID())
      return "";

    PresumedLoc PLoc = SourceMgr.getPresumedLoc(Loc);
    const char *Filename = PLoc.getFilename();
    int Line = PLoc.getLine();

    std::string lineStr;
    raw_string_ostream o(lineStr);
    o << "#line" << ' ' << Line << ' ' << '"';
    o.write_escaped(Filename);
    o << '"' << '\n';
    o.flush();
    return lineStr;
  }

private:
  Rewriter &TheRewriter;
  SourceManager &SourceMgr;
  TranslationUnitDecl *tu;
  RewriteHelper &helper;
  bool bNeedLineInfo;
};

HRESULT DoReWriteWithLineDirective(
    _In_ DxcLangExtensionsHelper *pExtHelper, _In_ LPCSTR pFileName,
    _In_ ASTUnit::RemappedFile *pRemap, _In_ hlsl::options::DxcOpts &opts,
    _In_ DxcDefine *pDefines, _In_ UINT32 defineCount, std::string &warnings,
    std::string &result, _In_opt_ dxcutil::DxcArgsFileSystem *msfPtr,
    IMalloc *pMalloc) {
  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  Rewriter rewriter;
  RewriteHelper rwHelper;
  ASTHelper astHelper;
  // Generate AST and rewrite the file.
  {
    GenerateAST(pExtHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
                opts, msfPtr, w);

    TranslationUnitDecl *tu = astHelper.tu;

    if (opts.EntryPoint.empty())
      opts.EntryPoint = "main";

    ASTContext &C = tu->getASTContext();
    rewriter.setSourceMgr(C.getSourceManager(), C.getLangOpts());
    if (opts.RWOpt.RemoveUnusedGlobals || opts.RWOpt.RemoveUnusedFunctions) {
      HRESULT hr = CollectRewriteHelper(tu, opts.EntryPoint.data(), rwHelper,
                           opts.RWOpt.RemoveUnusedGlobals,
                           opts.RWOpt.RemoveUnusedFunctions, w);
      if (hr == E_FAIL)
        return hr;
      RewriteVisitor visitor(rewriter, tu, rwHelper);
      visitor.TraverseDecl(tu);
    }
    // TODO: support ExtractEntryUniforms, GlobalExternByDefault, SkipStatic,
    // SkipFunctionBody.
    if (opts.RWOpt.ExtractEntryUniforms || opts.RWOpt.GlobalExternByDefault ||
        opts.RWOpt.SkipStatic || opts.RWOpt.SkipFunctionBody) {
      w << "-extract-entry-uniforms, -global-extern-by-default,-skip-static, "
           "-skip-fn-body are not supported yet when -line-directive is "
           "enabled";
      w.flush();
      return E_FAIL;
    }

    if (astHelper.bHasErrors) {
      o.flush();
      w.flush();
      return E_FAIL;
    }

  }
  // Preprocess rewritten files.
  {
    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(pMalloc, &pOutputStream));

    raw_stream_ostream outStream(pOutputStream.p);

    IFT(msfPtr->RegisterOutputStream(L"output.bc", pOutputStream));

    llvm::MemoryBuffer *pMemBuf = pRemap->second;
    std::unique_ptr<llvm::MemoryBuffer> pBuffer(
        llvm::MemoryBuffer::getMemBufferCopy(pMemBuf->getBuffer(), pFileName));

    std::unique_ptr<ASTUnit::RemappedFile> pPreprocessRemap(
        new ASTUnit::RemappedFile(pFileName, pBuffer.release()));
    // Need another compiler instance for preprocess because
    // PrintPreprocessedAction will createPreprocessor.
    CompilerInstance compiler;
    std::unique_ptr<TextDiagnosticPrinter> diagPrinter =
        llvm::make_unique<TextDiagnosticPrinter>(w,
                                                 &compiler.getDiagnosticOpts());
    SetupCompilerForPreprocess(compiler, pExtHelper, pFileName,
                               diagPrinter.get(), pPreprocessRemap.get(), opts,
                               pDefines, defineCount, msfPtr);

    auto &sourceManager = rewriter.getSourceMgr();
    auto &preprocessorOpts = compiler.getPreprocessorOpts();
    // Map rewrite buf to source manager of preprocessor compiler.
    for (auto it = rewriter.buffer_begin(); it != rewriter.buffer_end(); it++) {
      RewriteBuffer &buf = it->second;
      const FileEntry *Entry = sourceManager.getFileEntryForID(it->first);
      std::string lineStr;
      raw_string_ostream o(lineStr);
      buf.write(o);
      o.flush();
      StringRef fileName = Entry->getName();
      std::unique_ptr<llvm::MemoryBuffer> rewriteBuf =
          MemoryBuffer::getMemBufferCopy(lineStr, fileName);
      preprocessorOpts.addRemappedFile(fileName, rewriteBuf.release());
    }

    compiler.getFrontendOpts().OutputFile = "output.bc";
    compiler.WriteDefaultOutputDirectly = true;
    compiler.setOutStream(&outStream);
    try {
      PreprocessResult(compiler, pFileName);
      StringRef out((char *)pOutputStream.p->GetPtr(),
                    pOutputStream.p->GetPtrSize());
      o << out;
      compiler.setSourceManager(nullptr);
    } catch (Exception &exp) {
      w << exp.msg;
      return E_FAIL;
    } catch (...) {
      return E_FAIL;
    }
  }

  WriteMacroDefines(astHelper.semanticMacros, o);
  if (opts.RWOpt.KeepUserMacro)
    WriteMacroDefines(astHelper.userMacros, o);

  // Flush and return results.
  o.flush();
  w.flush();

  return S_OK;
}
} // namespace

class DxcRewriter : public IDxcRewriter2, public IDxcLangExtensions2 {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  DxcLangExtensionsHelper m_langExtensionsHelper;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcRewriter)
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langExtensionsHelper)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcRewriter2, IDxcRewriter,
                                 IDxcLangExtensions, IDxcLangExtensions2>(
        this, iid, ppvObject);
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

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem* msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(), utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      CW2A utf8EntryPoint(pEntryPoint, CP_UTF8);

      std::string errors;
      std::string rewrite;
      LPCWSTR pOutputName = nullptr;  // TODO: Fill this in
      HRESULT status = DoRewriteUnused(
          &m_langExtensionsHelper, fakeName, pRemap.get(), utf8EntryPoint,
          pDefines, defineCount, true /*removeGlobals*/,
          false /*removeFunctions*/, errors, rewrite, nullptr);
      return DxcResult::Create(status, DXC_OUT_HLSL, {
          DxcOutputObject::StringOutput(DXC_OUT_HLSL, CP_UTF8,  // TODO: Support DefaultTextCodePage
            rewrite.c_str(), pOutputName),
          DxcOutputObject::ErrorOutput(CP_UTF8,   // TODO Support DefaultTextCodePage
            errors.c_str())
        }, ppResult);
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

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem* msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(), utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      hlsl::options::DxcOpts opts;
      opts.HLSLVersion = 2015;

      std::string errors;
      std::string rewrite;
      HRESULT status =
          DoSimpleReWrite(&m_langExtensionsHelper, fakeName, pRemap.get(), opts,
                          pDefines, defineCount, errors, rewrite, nullptr);
      return DxcResult::Create(status, DXC_OUT_HLSL, {
          DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
            rewrite.c_str(), DxcOutNoName),
          DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage, errors.c_str())
        }, ppResult);
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

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    CW2A utf8SourceName(pSourceName, CP_UTF8);
    LPCSTR fName = utf8SourceName.m_psz;

    try {
      dxcutil::DxcArgsFileSystem *msfPtr = dxcutil::CreateDxcArgsFileSystem(utf8Source, pSourceName, pIncludeHandler);
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(), utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(llvm::MemoryBuffer::getMemBufferCopy(Data, fName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(new ASTUnit::RemappedFile(fName, pBuffer.release()));

      hlsl::options::DxcOpts opts;
      opts.HLSLVersion = 2015;

      opts.RWOpt.SkipFunctionBody |=
          rewriteOption & RewriterOptionMask::SkipFunctionBody;
      opts.RWOpt.SkipStatic |= rewriteOption & RewriterOptionMask::SkipStatic;
      opts.RWOpt.GlobalExternByDefault |=
          rewriteOption & RewriterOptionMask::GlobalExternByDefault;
      opts.RWOpt.KeepUserMacro |=
          rewriteOption & RewriterOptionMask::KeepUserMacro;

      std::string errors;
      std::string rewrite;
      HRESULT status =
          DoSimpleReWrite(&m_langExtensionsHelper, fName, pRemap.get(), opts,
                          pDefines, defineCount, errors, rewrite, msfPtr);
      return DxcResult::Create(status, DXC_OUT_HLSL, {
          DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
            rewrite.c_str(), DxcOutNoName),
          DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage, errors.c_str())
        }, ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();

  }

    HRESULT STDMETHODCALLTYPE RewriteWithOptions(
        _In_ IDxcBlobEncoding *pSource,
        // Optional file name for pSource. Used in errors and include handlers.
        _In_opt_ LPCWSTR pSourceName, 
        // Compiler arguments
        _In_count_(argCount) LPCWSTR *pArguments, _In_ UINT32 argCount, 
        // Defines
        _In_count_(defineCount) DxcDefine *pDefines, _In_ UINT32 defineCount,
        // user-provided interface to handle #include directives (optional)
        _In_opt_ IDxcIncludeHandler *pIncludeHandler,
        _COM_Outptr_ IDxcOperationResult **ppResult) override {

    if (pSource == nullptr || ppResult == nullptr ||
        (argCount > 0 && pArguments == nullptr) ||
        (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    CW2A utf8SourceName(pSourceName, CP_UTF8);
    LPCSTR fName = utf8SourceName.m_psz;

    try {
      dxcutil::DxcArgsFileSystem *msfPtr = dxcutil::CreateDxcArgsFileSystem(
          utf8Source, pSourceName, pIncludeHandler);
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(),
                     utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
          llvm::MemoryBuffer::getMemBufferCopy(Data, fName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(
          new ASTUnit::RemappedFile(fName, pBuffer.release()));

      hlsl::options::MainArgs mainArgs(argCount, pArguments, 0);
      hlsl::options::DxcOpts opts;
      IFR(ReadOptsAndValidate(mainArgs, opts, ppResult));
      HRESULT hr;
      if (*ppResult && SUCCEEDED((*ppResult)->GetStatus(&hr)) && FAILED(hr)) {
        // Looks odd, but this call succeeded enough to allocate a result
        return S_OK;
      }

      std::string errors;
      std::string rewrite;
      HRESULT status = S_OK;
      if (opts.RWOpt.WithLineDirective) {
        status = DoReWriteWithLineDirective(
            &m_langExtensionsHelper, fName, pRemap.get(), opts, pDefines,
            defineCount, errors, rewrite, msfPtr, m_pMalloc);
      } else {
        status = DoSimpleReWrite(&m_langExtensionsHelper, fName, pRemap.get(),
                                 opts, pDefines, defineCount, errors,
                                 rewrite, msfPtr);
      }
      return DxcResult::Create(status, DXC_OUT_HLSL, {
          DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
            rewrite.c_str(), DxcOutNoName),
          DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage, errors.c_str())
        }, ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

};

HRESULT CreateDxcRewriter(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcRewriter> isense = DxcRewriter::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(isense.p);
  return isense.p->QueryInterface(riid, ppv);
}
