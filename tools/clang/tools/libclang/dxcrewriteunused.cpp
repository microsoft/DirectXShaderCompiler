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
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Attr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/Support/Host.h"

#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"

#include "dxc/Support/DxcLangExtensionsHelper.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/dxcfilesystem.h"
#include "dxc/dxcapi.internal.h"
#include "dxc/dxctools.h"

#include "d3d12shader.h"

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
  SmallPtrSetImpl<VarDecl *> &m_unusedGlobals;
  SmallPtrSetImpl<FunctionDecl *> &m_visitedFunctions;
  SmallVectorImpl<FunctionDecl *> &m_pendingFunctions;
  SmallPtrSetImpl<TypeDecl *> &m_visitedTypes;

  void AddRecordType(TagDecl *tagDecl) {
    SaveTypeDecl(tagDecl, m_visitedTypes);
  }

public:
  VarReferenceVisitor(SmallPtrSetImpl<VarDecl *> &unusedGlobals,
                      SmallPtrSetImpl<FunctionDecl *> &visitedFunctions,
                      SmallVectorImpl<FunctionDecl *> &pendingFunctions,
                      SmallPtrSetImpl<TypeDecl *> &types)
      : m_unusedGlobals(unusedGlobals), m_visitedFunctions(visitedFunctions),
        m_pendingFunctions(pendingFunctions), m_visitedTypes(types) {}

  bool VisitDeclRefExpr(DeclRefExpr *ref) {
    ValueDecl *valueDecl = ref->getDecl();
    if (FunctionDecl *fnDecl = dyn_cast_or_null<FunctionDecl>(valueDecl)) {
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
    } else if (VarDecl *varDecl = dyn_cast_or_null<VarDecl>(valueDecl)) {
      m_unusedGlobals.erase(varDecl);
      if (TagDecl *tagDecl = varDecl->getType()->getAsTagDecl()) {
        AddRecordType(tagDecl);
      }
      if (Expr *initExp = varDecl->getInit()) {
        if (InitListExpr *initList = dyn_cast<InitListExpr>(initExp)) {
          TraverseInitListExpr(initList);
        } else if (ImplicitCastExpr *initCast =
                       dyn_cast<ImplicitCastExpr>(initExp)) {
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

// Collect all global constants.
class GlobalCBVisitor : public RecursiveASTVisitor<GlobalCBVisitor> {
private:
  SmallVectorImpl<VarDecl *> &globalConstants;

public:
  GlobalCBVisitor(SmallVectorImpl<VarDecl *> &globals)
      : globalConstants(globals) {}

  bool VisitVarDecl(VarDecl *vd) {
    // Skip local var.
    if (!vd->getDeclContext()->isTranslationUnit()) {
      auto *DclContext = vd->getDeclContext();
      while (NamespaceDecl *ND = dyn_cast<NamespaceDecl>(DclContext))
        DclContext = ND->getDeclContext();
      if (!DclContext->isTranslationUnit())
        return true;
    }
    // Skip group shared.
    if (vd->hasAttr<HLSLGroupSharedAttr>())
      return true;
    // Skip static global.
    if (!vd->hasExternalFormalLinkage())
      return true;
    // Skip resource.
    if (DXIL::ResourceClass::Invalid !=
        hlsl::GetResourceClassForType(vd->getASTContext(), vd->getType()))
      return true;

    globalConstants.emplace_back(vd);
    return true;
  }
};
// Collect types used by a record decl.
// TODO: template support.
class TypeVisitor : public RecursiveASTVisitor<TypeVisitor> {
private:
  MapVector<const TypeDecl *, DenseSet<const TypeDecl *>> &m_typeDepMap;

public:
  TypeVisitor(
      MapVector<const TypeDecl *, DenseSet<const TypeDecl *>> &typeDepMap)
      : m_typeDepMap(typeDepMap) {}

  bool VisitRecordType(const RecordType *RT) {
    RecordDecl *RD = RT->getDecl();
    if (m_typeDepMap.count(RD))
      return true;
    // Create empty dep set.
    m_typeDepMap[RD];
    if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
      for (const auto &I : CXXRD->bases()) {
        const CXXRecordDecl *BaseDecl =
            cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
        if (BaseDecl->field_empty())
          continue;
        QualType baseTy = QualType(BaseDecl->getTypeForDecl(), 0);
        TraverseType(baseTy);
        m_typeDepMap[RD].insert(BaseDecl);
      }
    }

    for (auto *field : RD->fields()) {
      QualType Ty = field->getType();
      if (hlsl::IsHLSLResourceType(Ty) || hlsl::IsHLSLNodeType(Ty) ||
          hlsl::IsHLSLVecMatType(Ty))
        continue;

      TraverseType(Ty);
      const clang::Type *TyPtr = Ty.getTypePtr();
      m_typeDepMap[RD].insert(TyPtr->getAsTagDecl());
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

bool ParsedSemanticDefineCompareIsLessThan(const ParsedSemanticDefine &left,
                                           const ParsedSemanticDefine &right) {
  return left.Name < right.Name;
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

ParsedSemanticDefineList
hlsl::CollectSemanticDefinesParsedByCompiler(CompilerInstance &compiler,
                                             DxcLangExtensionsHelper *helper) {
  ParsedSemanticDefineList parsedDefines;
  const llvm::SmallVector<std::string, 2> &defines =
      helper->GetSemanticDefines();
  if (defines.size() == 0) {
    return parsedDefines;
  }

  const llvm::SmallVector<std::string, 2> &defineExclusions =
      helper->GetSemanticDefineExclusions();

  const llvm::SetVector<std::string> &nonOptDefines =
      helper->GetNonOptSemanticDefines();

  std::set<std::string> overridenMacroSemDef;

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

      // overriding a semantic define takes the first precedence
      if (compiler.getCodeGenOpts().HLSLOverrideSemDefs.size() > 0 &&
          compiler.getCodeGenOpts().HLSLOverrideSemDefs.find(
              ii->getName().str()) !=
              compiler.getCodeGenOpts().HLSLOverrideSemDefs.end()) {
        std::string defName = ii->getName().str();
        std::string defValue =
            compiler.getCodeGenOpts().HLSLOverrideSemDefs[defName];
        overridenMacroSemDef.insert(defName);
        parsedDefines.emplace_back(ParsedSemanticDefine{defName, defValue, 0});
        continue;
      }

      // ignoring a specific semantic define takes second precedence
      if (compiler.getCodeGenOpts().HLSLIgnoreSemDefs.size() > 0 &&
          compiler.getCodeGenOpts().HLSLIgnoreSemDefs.find(
              ii->getName().str()) !=
              compiler.getCodeGenOpts().HLSLIgnoreSemDefs.end()) {
        continue;
      }

      // ignoring all non-correctness semantic defines takes third precendence
      if (compiler.getCodeGenOpts().HLSLIgnoreOptSemDefs &&
          !nonOptDefines.count(ii->getName().str())) {
        continue;
      }

      macros.push_back(std::pair<const IdentifierInfo *, MacroInfo *>(ii, mi));
    }
  }

  // If there are semantic defines which are passed using -override-semdef flag,
  // but we don't have that semantic define present in source or arglist, then
  // we just add the semantic define.
  for (auto &kv : compiler.getCodeGenOpts().HLSLOverrideSemDefs) {
    std::string overrideDefName = kv.first;
    std::string overrideDefVal = kv.second;
    if (overridenMacroSemDef.find(overrideDefName) ==
        overridenMacroSemDef.end()) {
      parsedDefines.emplace_back(
          ParsedSemanticDefine{overrideDefName, overrideDefVal, 0});
    }
  }

  if (!macros.empty()) {
    MacroExpander expander(pp);
    for (std::pair<const IdentifierInfo *, MacroInfo *> m : macros) {
      std::string expandedValue;
      expander.ExpandMacro(m.second, &expandedValue);
      parsedDefines.emplace_back(
          ParsedSemanticDefine{m.first->getName(), expandedValue,
                               m.second->getDefinitionLoc().getRawEncoding()});
    }
  }

  std::stable_sort(parsedDefines.begin(), parsedDefines.end(),
                   ParsedSemanticDefineCompareIsLessThan);
  return parsedDefines;
}

namespace {

void SetupCompilerCommon(CompilerInstance &compiler,
                         DxcLangExtensionsHelper *helper, LPCSTR pMainFile,
                         TextDiagnosticPrinter *diagPrinter,
                         ASTUnit::RemappedFile *rewrite,
                         hlsl::options::DxcOpts &opts) {
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
  compiler.getLangOpts().HLSLVersion = opts.HLSLVersion;
  compiler.getLangOpts().PreserveUnknownAnnotations = opts.RWOpt.ReflectHLSLBasics;
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

void SetupCompilerForRewrite(CompilerInstance &compiler,
                             DxcLangExtensionsHelper *helper, LPCSTR pMainFile,
                             TextDiagnosticPrinter *diagPrinter,
                             ASTUnit::RemappedFile *rewrite,
                             hlsl::options::DxcOpts &opts, LPCSTR pDefines,
                             dxcutil::DxcArgsFileSystem *msfPtr) {

  SetupCompilerCommon(compiler, helper, pMainFile, diagPrinter, rewrite, opts);

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

void SetupCompilerForPreprocess(CompilerInstance &compiler,
                                DxcLangExtensionsHelper *helper,
                                LPCSTR pMainFile,
                                TextDiagnosticPrinter *diagPrinter,
                                ASTUnit::RemappedFile *rewrite,
                                hlsl::options::DxcOpts &opts,
                                DxcDefine *pDefines, UINT32 defineCount,
                                dxcutil::DxcArgsFileSystem *msfPtr) {

  SetupCompilerCommon(compiler, helper, pMainFile, diagPrinter, rewrite, opts);

  if (pDefines) {
    PreprocessorOptions &PPOpts = compiler.getPreprocessorOpts();
    for (size_t i = 0; i < defineCount; ++i) {
      CW2A utf8Name(pDefines[i].Name);
      CW2A utf8Value(pDefines[i].Value);
      std::string val(utf8Name.m_psz);
      val += "=";
      val += (pDefines[i].Value) ? utf8Value.m_psz : "1";
      PPOpts.addMacroDef(val);
    }
  }
}

std::string DefinesToString(DxcDefine *pDefines, UINT32 defineCount) {
  std::string defineStr;
  for (UINT32 i = 0; i < defineCount; i++) {
    CW2A utf8Name(pDefines[i].Name);
    CW2A utf8Value(pDefines[i].Value);
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

static HRESULT ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                                   hlsl::options::DxcOpts &opts,
                                   IDxcOperationResult **ppResult) {
  const llvm::opt::OptTable *table = ::options::getHlslOptTable();

  CComPtr<AbstractMemoryStream> pOutputStream;
  IFT(CreateMemoryStream(GetGlobalHeapMalloc(), &pOutputStream));
  raw_stream_ostream outStream(pOutputStream);

  if (0 != hlsl::options::ReadDxcOpts(table,
                                      hlsl::options::HlslFlags::RewriteOption,
                                      mainArgs, opts, outStream)) {
    CComPtr<IDxcBlob> pErrorBlob;
    IFT(pOutputStream->QueryInterface(&pErrorBlob));
    outStream.flush();
    IFT(DxcResult::Create(
        E_INVALIDARG, DXC_OUT_NONE,
        {DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                      (LPCSTR)pErrorBlob->GetBufferPointer(),
                                      pErrorBlob->GetBufferSize())},
        ppResult));
    return S_OK;
  }
  return S_OK;
}

static bool HasUniformParams(FunctionDecl *FD) {
  for (auto PD : FD->params()) {
    if (PD->hasAttr<HLSLUniformAttr>())
      return true;
  }
  return false;
}

static void WriteUniformParamsAsGlobals(FunctionDecl *FD, raw_ostream &o,
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

static void PrintTranslationUnitWithTranslatedUniformParams(
    TranslationUnitDecl *tu, FunctionDecl *entryFnDecl, raw_ostream &o,
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

static HRESULT DoRewriteUnused(TranslationUnitDecl *tu, LPCSTR pEntryPoint,
                               bool bRemoveGlobals, bool bRemoveFunctions,
                               raw_ostream &w) {
  RewriteHelper helper;
  HRESULT hr = CollectRewriteHelper(tu, pEntryPoint, helper, bRemoveGlobals,
                                    bRemoveFunctions, w);
  if (hr != S_OK)
    return hr;

  // Remove all unused variables and functions.
  for (VarDecl *unusedGlobal : helper.unusedGlobals) {
    if (const RecordType *recordTy =
            unusedGlobal->getType()->getAs<RecordType>()) {
      RecordDecl *recordDecl = recordTy->getDecl();
      if (recordDecl && recordDecl->getName().empty()) {
        // Anonymous structs can only be referenced by the variable they
        // declare. If we've removed all declared variables of such a struct,
        // remove it too, because anonymous structs without variable
        // declarations in global scope are illegal.
        auto recordRefCountIter =
            helper.anonymousRecordRefCounts.find(recordDecl);
        DXASSERT_NOMSG(recordRefCountIter !=
                           helper.anonymousRecordRefCounts.end() &&
                       recordRefCountIter->second > 0);
        recordRefCountIter->second--;
        if (recordRefCountIter->second == 0) {
          tu->removeDecl(recordDecl);
          helper.anonymousRecordRefCounts.erase(recordRefCountIter);
        }
      }
    }
    if (HLSLBufferDecl *CBV =
            dyn_cast<HLSLBufferDecl>(unusedGlobal->getLexicalDeclContext())) {
      if (CBV->isConstantBufferView()) {
        // For constant buffer view, we create a variable for the constant.
        // The variable use tu as the DeclContext to access as global variable,
        // CBV as LexicalDeclContext so it is still part of CBV.
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

static HRESULT DoRewriteUnused(DxcLangExtensionsHelper *pHelper,
                               LPCSTR pFileName, ASTUnit::RemappedFile *pRemap,
                               LPCSTR pEntryPoint, DxcDefine *pDefines,
                               UINT32 defineCount, bool bRemoveGlobals,
                               bool bRemoveFunctions, std::string &warnings,
                               std::string &result,
                               dxcutil::DxcArgsFileSystem *msfPtr) {

  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;
  hlsl::options::DxcOpts opts;
  opts.HLSLVersion = hlsl::LangStd::v2015;

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
  for (auto it = Ctx.decls_begin(); it != Ctx.decls_end();) {
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

struct ResourceKey {

  uint32_t space;
  DXIL::ResourceClass resourceClass;

  bool operator==(const ResourceKey &other) const {
    return space == other.space && resourceClass == other.resourceClass;
  }
};

namespace llvm {
template <> struct DenseMapInfo<ResourceKey> {
  static inline ResourceKey getEmptyKey() {
    return {~0u, DXIL::ResourceClass::Invalid};
  }
  static inline ResourceKey getTombstoneKey() {
    return {~0u - 1, DXIL::ResourceClass::Invalid};
  }
  static unsigned getHashValue(const ResourceKey &K) {
    return llvm::hash_combine(K.space, uint32_t(K.resourceClass));
  }
  static bool isEqual(const ResourceKey &LHS, const ResourceKey &RHS) {
    return LHS.space == RHS.space && LHS.resourceClass == RHS.resourceClass;
  }
};
} // namespace llvm

using RegisterRange = std::pair<uint32_t, uint32_t>; //(startReg, count)
using RegisterMap =
    llvm::DenseMap<ResourceKey, llvm::SmallVector<RegisterRange, 8>>;

struct UnresolvedRegister {
  hlsl::DXIL::ResourceClass cls;
  uint32_t arraySize;
  RegisterAssignment *reg;
  NamedDecl *ND;
};

using UnresolvedRegisters = llvm::SmallVector<UnresolvedRegister, 8>;

// Find gap in register list and fill it

//TODO: Niels, check multi dim arrays

uint32_t FillNextRegister(llvm::SmallVector<RegisterRange, 8> &ranges,
                          uint32_t arraySize) {

  if (ranges.empty()) {
    ranges.push_back({0, arraySize});
    return 0;
  }

  size_t i = 0, j = ranges.size();
  size_t curr = 0;

  for (; i < j; ++i) {

    const RegisterRange &range = ranges[i];

    if (range.first - curr >= arraySize) {
      ranges.insert(ranges.begin() + i, RegisterRange{curr, arraySize});
      return curr;
    }

    curr = range.first + range.second;
  }

  ranges.emplace_back(RegisterRange{curr, arraySize});
  return curr;
}

// Insert in the right place (keep sorted)

void FillConsistentRegisterAt(llvm::SmallVector<RegisterRange, 8> &ranges,
                    uint32_t registerNr, uint32_t arraySize,
                    clang::DiagnosticsEngine &diags,
                    const SourceLocation &location) {

  size_t i = 0, j = ranges.size();

  for (; i < j; ++i) {

    const RegisterRange &range = ranges[i];

    if (range.first > registerNr) {

      if (registerNr + arraySize > range.first) {
        diags.Report(location, diag::err_hlsl_register_semantics_conflicting);
        return;
      }

      ranges.insert(ranges.begin() + i, RegisterRange{registerNr, arraySize});
      break;
    }

    if (range.first + range.second > registerNr) {
      diags.Report(location, diag::err_hlsl_register_semantics_conflicting);
      return;
    }
  }

  if (i == j)
    ranges.emplace_back(RegisterRange{registerNr, arraySize});
}

static void RegisterConsistentBinding(NamedDecl *ND,
                            UnresolvedRegisters &unresolvedRegisters,
                            RegisterMap &map, hlsl::DXIL::ResourceClass cls,
                            uint32_t arraySize, clang::DiagnosticsEngine &Diags,
                            uint32_t autoBindingSpace) {

  const ArrayRef<hlsl::UnusualAnnotation *> &UA = ND->getUnusualAnnotations();

  bool qualified = false;
  RegisterAssignment *reg = nullptr;

  for (auto It = UA.begin(), E = UA.end(); It != E; ++It) {

    if ((*It)->getKind() != hlsl::UnusualAnnotation::UA_RegisterAssignment)
      continue;

    reg = cast<hlsl::RegisterAssignment>(*It);

    if (!reg->RegisterType) // Unqualified register assignment
      break;

    uint32_t space = reg->RegisterSpace.hasValue()
                         ? reg->RegisterSpace.getValue()
                         : autoBindingSpace;

    qualified = true;
    FillConsistentRegisterAt(map[ResourceKey{space, cls}], reg->RegisterNumber, arraySize,
                   Diags, ND->getLocation());
    break;
  }

  if (!qualified)
    unresolvedRegisters.emplace_back(
        UnresolvedRegister{cls, arraySize, reg, ND});
}

static void GenerateConsistentBindings(DeclContext &Ctx,
                                       uint32_t autoBindingSpace) {

  clang::DiagnosticsEngine &Diags = Ctx.getParentASTContext().getDiagnostics();

  RegisterMap map;
  UnresolvedRegisters unresolvedRegisters;

  // Fill up map with fully qualified registers to avoid colliding with them
  // later

  for (Decl *it : Ctx.decls()) {

    // CBuffer has special logic, since it's not technically

    if (HLSLBufferDecl *CBuffer = dyn_cast<HLSLBufferDecl>(it)) {
      RegisterConsistentBinding(CBuffer, unresolvedRegisters, map,
                      hlsl::DXIL::ResourceClass::CBuffer, 1, Diags,
                      autoBindingSpace);
      continue;
    }

    ValueDecl *VD = dyn_cast<ValueDecl>(it);

    if (!VD)
      continue;

    std::string test = VD->getName();

    uint32_t arraySize = 1;
    QualType type = VD->getType();

    while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(type)) {
      arraySize *= arr->getSize().getZExtValue();
      type = arr->getElementType();
    }

    if (!IsHLSLResourceType(type))
      continue;

    RegisterConsistentBinding(VD, unresolvedRegisters, map, GetHLSLResourceClass(type),
                    arraySize, Diags, autoBindingSpace);
  }

  // Resolve unresolved registers (while avoiding collisions)

  for (const UnresolvedRegister &reg : unresolvedRegisters) {

    uint32_t arraySize = reg.arraySize;
    hlsl::DXIL::ResourceClass resClass = reg.cls;

    char prefix = 't';

    switch (resClass) {

    case DXIL::ResourceClass::Sampler:
      prefix = 's';
      break;

    case DXIL::ResourceClass::CBuffer:
      prefix = 'b';
      break;

    case DXIL::ResourceClass::UAV:
      prefix = 'u';
      break;
    }

    uint32_t space =
        reg.reg ? reg.reg->RegisterSpace.getValue() : autoBindingSpace;

    uint32_t registerNr =
        FillNextRegister(map[ResourceKey{space, resClass}], arraySize);

    if (reg.reg) {
      reg.reg->RegisterType = prefix;
      reg.reg->RegisterNumber = registerNr;
      reg.reg->setIsValid(true);
    } else {
      hlsl::RegisterAssignment
          r; // Keep space empty to ensure space overrides still work fine
      r.RegisterNumber = registerNr;
      r.RegisterType = prefix;
      r.setIsValid(true);

      llvm::SmallVector<UnusualAnnotation *, 8> annotations;

      const ArrayRef<hlsl::UnusualAnnotation *> &UA =
          reg.ND->getUnusualAnnotations();

      for (auto It = UA.begin(), E = UA.end(); It != E; ++It)
        annotations.emplace_back(*It);

      annotations.push_back(::new (Ctx.getParentASTContext())
                                hlsl::RegisterAssignment(r));

      reg.ND->setUnusualAnnotations(UnusualAnnotation::CopyToASTContextArray(
          Ctx.getParentASTContext(), annotations.data(), annotations.size()));
    }
  }
}

enum class DxcHLSLNodeType : uint64_t {
  Register,
  Function,
  Enum,
  EnumValue,
  Namespace,
  Typedef,
  Using,
  Variable,         //localId points to the type for a variable
  Parameter,
  Type,

  Start = Register,
  End = Type
};

struct DxcHLSLNode {

  union {
    struct {
      uint32_t NameId; // Local name (not including parent's name)

      uint16_t FileNameId;
      uint16_t SourceLineCount;
    };
    uint64_t NameIdFileNameIdSourceLineCount;
  };

  uint64_t LocalIdAnnotationSourceLineStart;

  uint64_t TypeChildsParentAnnotationsSourceColumnHi;

  union {
    struct {
      uint16_t SourceColumnStartLo;
      uint16_t SourceColumnEndLo;
      uint32_t Padding;
    };
    uint64_t SourceColumnStartEndLo;
  };

  DxcHLSLNode() = default;

  DxcHLSLNode(uint32_t NameId,

              DxcHLSLNodeType NodeType, uint32_t LocalId,
              uint32_t AnnotationStart, uint16_t FileNameId,

              uint32_t ChildCount, uint32_t ParentId, uint16_t SourceLineCount,

              uint32_t SourceLineStart, uint32_t SourceColumnStart,
              uint32_t SourceColumnEnd, uint16_t AnnotationCount)
      : NameId(NameId), FileNameId(FileNameId),
        SourceLineCount(SourceLineCount),
        SourceColumnStartLo(uint16_t(SourceColumnStart)),
        SourceColumnEndLo(uint16_t(SourceColumnEnd)),
        LocalIdAnnotationSourceLineStart((uint64_t(LocalId) << (64 - 24)) |
                                         (uint64_t(AnnotationStart) << 20) |
                                         SourceLineStart),
        TypeChildsParentAnnotationsSourceColumnHi((uint64_t(NodeType) << 60) |
                                                  (uint64_t(ChildCount) << (10 + 2 + 24)) |
                                                  (uint64_t(AnnotationCount) << (2 + 24)) |
                                                  (SourceColumnStart >> 16 << 24) |
                                                  (SourceColumnEnd >> 16 << 25) |
                                                  (ParentId)) {

    assert(NodeType >= DxcHLSLNodeType::Start &&
           NodeType <= DxcHLSLNodeType::End && "Invalid enum value");

    assert(LocalId < ((1 << 24) - 1) && "LocalId out of bounds");
    assert(ParentId < ((1 << 24) - 1) && "ParentId out of bounds");
    assert(ChildCount < ((1 << 24) - 1) && "ChildCount out of bounds");
    assert(SourceColumnStart < (1 << 17) && "SourceColumnStart out of bounds");
    assert(SourceColumnEnd < (1 << 17) && "SourceColumnEnd out of bounds");
    assert(AnnotationCount < (1 << 10) && "AnnotationCount out of bounds");

    assert(AnnotationStart < ((1 << 20) - 1) &&
           "AnnotationStart out of bounds");

    assert(SourceLineStart < ((1 << 20) - 1) &&
           "SourceLineStart out of bounds");
  }

  // For example if Enum, maps into Enums[LocalId]
  uint32_t GetLocalId() const {
    return LocalIdAnnotationSourceLineStart >> (64 - 24);
  }

  uint32_t GetAnnotationStart() const {
    return uint32_t(LocalIdAnnotationSourceLineStart << 24 >> (64 - 20));
  }

  uint32_t GetSourceLineStart() const {
    return uint32_t(LocalIdAnnotationSourceLineStart & ((1 << 20) - 1));
  }

  DxcHLSLNodeType GetNodeType() const {
    return DxcHLSLNodeType(TypeChildsParentAnnotationsSourceColumnHi >> 60);
  }

  // Includes recursive children
  uint32_t GetChildCount() const {
    return uint32_t(TypeChildsParentAnnotationsSourceColumnHi << 4 >>
                    (64 - 24));
  }

  uint32_t GetAnnotationCount() const {
    return uint32_t(TypeChildsParentAnnotationsSourceColumnHi << (4 + 24) >>
                    (64 - 10));
  }

  uint32_t GetParentId() const {
    return uint32_t(TypeChildsParentAnnotationsSourceColumnHi &
                    ((1 << 24) - 1));
  }

  uint32_t GetSourceColumnStart() const {
    return SourceColumnStartLo |
           ((TypeChildsParentAnnotationsSourceColumnHi & (1 << 24)) >>
            (24 - 16));
  }

  uint32_t GetSourceColumnEnd() const {
    return SourceColumnEndLo |
           ((TypeChildsParentAnnotationsSourceColumnHi & (1 << 25)) >>
            (25 - 16));
  }

  void IncreaseChildCount() {
    uint32_t childCount = GetChildCount();
    assert(childCount < ((1 << 24) - 1) && "Child count out of bounds");
    TypeChildsParentAnnotationsSourceColumnHi += uint64_t(1) << (10 + 2 + 24);
  }

  bool operator==(const DxcHLSLNode &other) const {
    return NameIdFileNameIdSourceLineCount ==
               other.NameIdFileNameIdSourceLineCount &&
           SourceColumnStartEndLo == other.SourceColumnStartEndLo &&
           TypeChildsParentAnnotationsSourceColumnHi ==
               other.TypeChildsParentAnnotationsSourceColumnHi &&
           LocalIdAnnotationSourceLineStart ==
               other.LocalIdAnnotationSourceLineStart;
  }
};

struct DxcHLSLEnumDesc {

  uint32_t NodeId;
  D3D12_HLSL_ENUM_TYPE Type;

  bool operator==(const DxcHLSLEnumDesc &other) const {
    return NodeId == other.NodeId && Type == other.Type;
  }
};

struct DxcHLSLEnumValue {

  int64_t Value;
  uint32_t NodeId;

  bool operator==(const DxcHLSLEnumValue &other) const {
    return Value == other.Value &&
           NodeId == other.NodeId;
  }
};

struct DxcHLSLParameter { // Mirrors D3D12_PARAMETER_DESC (ex.
                          // First(In/Out)(Register/Component)), but with
                          // std::string and NodeId

  std::string SemanticName;
  D3D_SHADER_VARIABLE_TYPE Type;            // Element type.
  D3D_SHADER_VARIABLE_CLASS Class;          // Scalar/Vector/Matrix.
  uint32_t Rows;                            // Rows are for matrix parameters.
  uint32_t Columns;                         // Components or Columns in matrix.
  D3D_INTERPOLATION_MODE InterpolationMode; // Interpolation mode.
  D3D_PARAMETER_FLAGS Flags;                // Parameter modifiers.
  uint32_t NodeId;

  // TODO: Array info
};

struct DxcHLSLFunction {

  uint32_t NodeId;
  uint32_t NumParametersHasReturnAndDefinition;

  DxcHLSLFunction() = default;

  DxcHLSLFunction(uint32_t NodeId, uint32_t NumParameters, bool HasReturn,
                  bool HasDefinition)
      : NodeId(NodeId),
        NumParametersHasReturnAndDefinition(NumParameters |
                                            (HasReturn ? (1 << 30) : 0) |
                                            (HasDefinition ? (1 << 31) : 0)) {

    assert(NumParameters < (1 << 30) && "NumParameters out of bounds");
  }

  uint32_t GetNumParameters() const {
    return NumParametersHasReturnAndDefinition << 2 >> 2;
  }

  bool HasReturn() const {
    return (NumParametersHasReturnAndDefinition >> 30) & 1;
  }

  bool HasDefinition() const {
    return (NumParametersHasReturnAndDefinition >> 31) & 1;
  }

  bool operator==(const DxcHLSLFunction &other) const {
    return NodeId == other.NodeId &&
           NumParametersHasReturnAndDefinition ==
               other.NumParametersHasReturnAndDefinition;
  }
};

struct DxcHLSLRegister { // Almost maps to D3D12_SHADER_INPUT_BIND_DESC, minus
                         // the Name (and uID replaced with NodeID) and added
                         // arrayIndex and better packing

  union {
    struct {
      uint8_t Type;       // D3D_SHADER_INPUT_TYPE
      uint8_t Dimension;  // D3D_SRV_DIMENSION
      uint8_t ReturnType; // D3D_RESOURCE_RETURN_TYPE
      uint8_t uFlags;

      uint32_t BindPoint;
    };
    uint64_t TypeDimensionReturnTypeFlagsBindPoint;
  };

  union {
    struct {
      uint32_t Space;
      uint32_t BindCount;
    };
    uint64_t SpaceBindCount;
  };

  union {
    struct {
      uint32_t NumSamples;
      uint32_t NodeId;
    };
    uint64_t NumSamplesNodeId;
  };

  union {
    struct {
      uint32_t ArrayId;  // Only if BindCount > 1 and the array is 2D+ (else -1)
      uint32_t BufferId; // If cbuffer or structured buffer
    };
    uint64_t ArrayIdBufferId;
  };

  DxcHLSLRegister() = default;
  DxcHLSLRegister(D3D_SHADER_INPUT_TYPE Type, uint32_t BindPoint,
                  uint32_t BindCount, uint32_t uFlags,
                  D3D_RESOURCE_RETURN_TYPE ReturnType,
                  D3D_SRV_DIMENSION Dimension, uint32_t NumSamples,
                  uint32_t Space, uint32_t NodeId, uint32_t ArrayId,
                  uint32_t BufferId)
      : Type(Type), BindPoint(BindPoint), BindCount(BindCount), uFlags(uFlags),
        ReturnType(ReturnType), Dimension(Dimension),
        NumSamples(NumSamples), Space(Space), NodeId(NodeId),
        ArrayId(ArrayId), BufferId(BufferId) {

    assert(Type >= D3D_SIT_CBUFFER && Type <= D3D_SIT_UAV_FEEDBACKTEXTURE &&
           "Invalid type");

    assert(ReturnType >= 0 && ReturnType <= D3D_RETURN_TYPE_CONTINUED &&
           "Invalid return type");

    assert(Dimension >= D3D_SRV_DIMENSION_UNKNOWN &&
           Dimension <= D3D_SRV_DIMENSION_BUFFEREX && "Invalid srv dimension");

    assert(!(uFlags >> 8) && "Invalid user flags");
  }

  bool operator==(const DxcHLSLRegister &other) const {
    return TypeDimensionReturnTypeFlagsBindPoint ==
               other.TypeDimensionReturnTypeFlagsBindPoint &&
           SpaceBindCount == other.SpaceBindCount &&
           NumSamplesNodeId == other.NumSamplesNodeId &&
           ArrayIdBufferId == other.ArrayIdBufferId;
  }
};

struct DxcHLSLArray {

  uint32_t ArrayElemStart;

  DxcHLSLArray() = default;
  DxcHLSLArray(uint32_t ArrayElem, uint32_t ArrayStart)
      : ArrayElemStart((ArrayElem << 28) | ArrayStart) {

    assert(ArrayElem <= 8 && ArrayElem > 1 && "ArrayElem out of bounds");
    assert(ArrayStart < (1 << 28) && "ArrayStart out of bounds");
  }

  bool operator==(const DxcHLSLArray &Other) const {
    return Other.ArrayElemStart == ArrayElemStart;
  }

  uint32_t ArrayElem() const { return ArrayElemStart >> 28; }
  uint32_t ArrayStart() const { return ArrayElemStart << 4 >> 4; }
};

struct DxcHLSLMember {

  uint32_t NameId;
  uint32_t TypeId;

  bool operator==(const DxcHLSLMember &Other) const {
    return Other.NameId == NameId && Other.TypeId == TypeId;
  }
};

struct DxcHLSLType { // Almost maps to CShaderReflectionType and
                     // D3D12_SHADER_TYPE_DESC, but tightly packed and
                     // easily serializable
  union {
    struct {
      uint32_t MembersCount;
      uint32_t MembersStart;
    };
    uint64_t MembersData;
  };

  union {
    struct {
      uint32_t NameId; // Can be empty
      uint8_t Class;   // D3D_SHADER_VARIABLE_CLASS
      uint8_t Type;    // D3D_SHADER_VARIABLE_TYPE
      uint8_t Rows;
      uint8_t Columns;
    };
    uint64_t NameIdClassTypeRowsColums;
  };

  union {
    struct {
      uint32_t ElementsOrArrayId;
      uint32_t BaseClass; // -1 if none, otherwise a type index
    };
    uint64_t ElementsOrArrayIdBaseClass;
  };

  bool operator==(const DxcHLSLType &Other) const {
    return Other.MembersData == MembersData &&
           NameIdClassTypeRowsColums == Other.NameIdClassTypeRowsColums &&
           ElementsOrArrayIdBaseClass == Other.ElementsOrArrayIdBaseClass;
  }

  bool IsMultiDimensionalArray() const { return ElementsOrArrayId >> 31; }
  bool IsArray() const { return ElementsOrArrayId; }
  bool Is1DArray() const { return IsArray() && !IsMultiDimensionalArray(); }

  uint32_t Get1DElements() const {
    return IsMultiDimensionalArray() ? 0 : ElementsOrArrayId;
  }

  uint32_t GetMultiDimensionalArrayId() const {
    return IsMultiDimensionalArray() ? (ElementsOrArrayId << 1 >> 1)
                                     : (uint32_t)-1;
  }

  DxcHLSLType() = default;
  DxcHLSLType(uint32_t NameId, uint32_t BaseClass, uint32_t ElementsOrArrayId,
              D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type,
              uint8_t Rows, uint8_t Columns, uint32_t MembersCount,
              uint32_t MembersStart)
      : MembersStart(MembersStart), MembersCount(MembersCount), NameId(NameId),
        Class(Class), Type(Type), Rows(Rows), Columns(Columns),
        ElementsOrArrayId(ElementsOrArrayId), BaseClass(BaseClass) {

    assert(Class >= D3D_SVC_SCALAR && Class <= D3D_SVC_INTERFACE_POINTER &&
           "Invalid class");
    assert(Type >= D3D_SVT_VOID && Type <= D3D_SVT_UINT64 && "Invalid type");
  }
};

struct DxcRegisterTypeInfo {
  D3D_SHADER_INPUT_TYPE RegisterType;
  D3D_SHADER_INPUT_FLAGS RegisterFlags;
  D3D_SRV_DIMENSION TextureDimension;
  D3D_RESOURCE_RETURN_TYPE TextureValue;
  uint32_t SampleCount;
};

struct DxcHLSLBuffer { // Almost maps to CShaderReflectionConstantBuffer and
                       // D3D12_SHADER_BUFFER_DESC

  D3D_CBUFFER_TYPE Type;
  uint32_t NodeId;

  bool operator==(const DxcHLSLBuffer &other) const {
    return Type == other.Type && NodeId == other.NodeId;
  }
};

struct DxcReflectionData {

  std::vector<std::string> Strings;
  std::unordered_map<std::string, uint32_t> StringsToId;

  std::vector<uint32_t> Sources;
  std::unordered_map<std::string, uint16_t> StringToSourceId;

  std::vector<DxcHLSLNode> Nodes;       //0 = Root node (global scope)

  std::vector<DxcHLSLRegister> Registers;
  std::vector<DxcHLSLFunction> Functions;

  std::vector<DxcHLSLEnumDesc> Enums;
  std::vector<DxcHLSLEnumValue> EnumValues;

  //std::vector<DxcHLSLParameter> Parameters;
  std::vector<uint32_t> Annotations;

  std::vector<DxcHLSLArray> Arrays;
  std::vector<uint32_t> ArraySizes;

  std::vector<DxcHLSLMember> Members;
  std::vector<DxcHLSLType> Types;
  std::vector<DxcHLSLBuffer> Buffers;

  void Dump(std::vector<std::byte> &Bytes) const;

  DxcReflectionData() = default;
  DxcReflectionData(const std::vector<std::byte> &Bytes);

  bool operator==(const DxcReflectionData& other) const {
    return Strings == other.Strings && Sources == other.Sources &&
           Nodes == other.Nodes && Registers == other.Registers &&
           Functions == other.Functions && Enums == other.Enums &&
           EnumValues == other.EnumValues && Annotations == other.Annotations &&
           Arrays == other.Arrays && ArraySizes == other.ArraySizes &&
           Members == other.Members && Types == other.Types &&
           Buffers == other.Buffers;
  }
};

static uint32_t RegisterString(DxcReflectionData &Refl,
    const std::string &Name) {

  assert(Refl.Strings.size() < (uint32_t)-1 && "Strings overflow");
  assert(Name.size() < 32768 && "Strings are limited to 32767");

  auto it = Refl.StringsToId.find(Name);

  if (it != Refl.StringsToId.end())
    return it->second;

  uint32_t stringId = (uint32_t) Refl.Strings.size();

  Refl.Strings.push_back(Name);
  Refl.StringsToId[Name] = stringId;
  return stringId;
}

static uint32_t PushNextNodeId(DxcReflectionData &Refl, const SourceManager &SM,
                               const LangOptions &LangOpts,
                               const std::string &UnqualifiedName, Decl *Decl,
                               DxcHLSLNodeType Type, uint32_t ParentNodeId,
                               uint32_t LocalId, const SourceRange *Range = nullptr) {

  assert(Refl.Nodes.size() < (uint32_t)(1 << 24) && "Nodes overflow");
  assert(LocalId < (uint32_t)(1 << 24) && "LocalId overflow");

  uint32_t nodeId = Refl.Nodes.size();

  uint32_t annotationStart = (uint32_t) Refl.Annotations.size();
  uint16_t annotationCount = 0;

  if (Decl) {
    for (const Attr *attr : Decl->attrs()) {
      if (const AnnotateAttr *annotate = dyn_cast<AnnotateAttr>(attr)) {
        assert(Refl.Annotations.size() < (1 << 20) && "Out of annotations");
        Refl.Annotations.push_back(
            RegisterString(Refl, annotate->getAnnotation().str()));
        assert(annotationCount != uint16_t(-1) &&
               "Annotation count out of bounds");
        ++annotationCount;
      }
    }
  }

  uint16_t sourceLineCount = 0;
  uint32_t sourceLineStart = 0;
  uint32_t sourceColumnStart = 0;
  uint32_t sourceColumnEnd = 0;

  uint16_t fileNameId = (uint16_t)-1;

  SourceRange range =
      Decl ? Decl->getSourceRange() : (Range ? *Range : SourceRange());

  SourceLocation start = range.getBegin();
  SourceLocation end = range.getEnd();

  if (start.isValid() && end.isValid()) {

    PresumedLoc presumed = SM.getPresumedLoc(start);

    SourceLocation realEnd = SM.getFileLoc(end);
    SourceLocation endOfToken =
        Lexer::getLocForEndOfToken(realEnd, 0, SM, LangOpts);
    PresumedLoc presumedEnd = SM.getPresumedLoc(endOfToken);

    if (presumed.isValid() && presumedEnd.isValid()) {

      uint32_t startLine = presumed.getLine();
      uint32_t startCol = presumed.getColumn();
      uint32_t endLine = presumedEnd.getLine();
      uint32_t endCol = presumedEnd.getColumn();

      std::string fileName = presumed.getFilename();

      assert(fileName == presumedEnd.getFilename() &&
             "End and start are not in the same file");

      auto it = Refl.StringToSourceId.find(fileName);
      uint32_t i;

      if (it == Refl.StringToSourceId.end()) {
        i = (uint32_t)Refl.Sources.size();
        Refl.Sources.push_back(RegisterString(Refl, fileName));
        Refl.StringToSourceId[fileName] = i;
      }

      else {
        i = it->second;
      }

      assert(i < 65535 && "Source file count is limited to 16-bit");
      assert((endLine - startLine) < 65535 &&
             "Source line count is limited to 16-bit");
      assert(startLine < 1048576 && "Source line start is limited to 20-bit");
      assert(startCol < 131072 && "Column start is limited to 17-bit");
      assert(endCol < 131072 && "Column end is limited to 17-bit");

      sourceLineCount = uint16_t(endLine - startLine + 1);
      sourceLineStart = startLine;
      sourceColumnStart = startCol;
      sourceColumnEnd = endCol;
      fileNameId = (uint16_t)i;
    }
  }

  uint32_t nameId = RegisterString(Refl, UnqualifiedName);

  Refl.Nodes.push_back(DxcHLSLNode{nameId, Type, LocalId, annotationStart, fileNameId,
                        0, ParentNodeId, sourceLineCount, sourceLineStart,
                        sourceColumnStart, sourceColumnEnd, annotationCount});

  uint32_t parentParent = ParentNodeId;

  while (parentParent != 0) {
    DxcHLSLNode &parent = Refl.Nodes[parentParent];
    parent.IncreaseChildCount();
    parentParent = parent.GetParentId();
  }

  Refl.Nodes[0].IncreaseChildCount();

  return nodeId;
}

static DxcRegisterTypeInfo GetTextureRegisterInfo(ASTContext &ASTCtx,
                                                  std::string TypeName,
                                                  bool IsWrite,
                                                  const CXXRecordDecl *RecordDecl) {
    
  DxcRegisterTypeInfo type = {};
  type.RegisterType = IsWrite ? D3D_SIT_UAV_RWTYPED : D3D_SIT_TEXTURE;
  type.SampleCount = (uint32_t)-1;

  //Parse return type and dimensions

  const ClassTemplateSpecializationDecl *textureTemplate =
      dyn_cast<ClassTemplateSpecializationDecl>(RecordDecl);

  assert(textureTemplate && "Expected texture template");

  const ArrayRef<TemplateArgument>& textureParams = textureTemplate->getTemplateArgs().asArray();

  assert(textureParams.size() == 1 && !textureParams[0].getAsType().isNull() &&
         "Expected template args");

  QualType valueType = textureParams[0].getAsType();
  QualType desugared = valueType.getDesugaredType(ASTCtx);

  const RecordType *RT = desugared->getAs<RecordType>();
  assert(RT && "Expected record type");

  const CXXRecordDecl *RD = dyn_cast<CXXRecordDecl>(RT->getDecl());
  assert(RT && "Expected record decl");

  const ClassTemplateSpecializationDecl *vectorType =
      dyn_cast<ClassTemplateSpecializationDecl>(RD);

  assert(vectorType &&
         "Expected vector type as template inside of texture template");

  const ArrayRef<TemplateArgument> &vectorParams =
      vectorType->getTemplateArgs().asArray();

  assert(vectorParams.size() == 2 && !vectorParams[0].getAsType().isNull() &&
         vectorParams[1].getKind() == TemplateArgument::Integral &&
         "Expected vector to be vector<T, N>");

  valueType = vectorParams[0].getAsType();
  desugared = valueType.getDesugaredType(ASTCtx);

  if (desugared->isFloatingType()) {
    type.TextureValue = desugared->isSpecificBuiltinType(BuiltinType::Double)
                            ? D3D_RETURN_TYPE_DOUBLE
                            : D3D_RETURN_TYPE_FLOAT;
  } else if (desugared->isIntegerType()) {
    const auto &semantics = ASTCtx.getTypeInfo(desugared);
    if (semantics.Width == 64) {
      type.TextureValue = D3D_RETURN_TYPE_MIXED;
    } else {
      type.TextureValue = desugared->isUnsignedIntegerType()
                              ? D3D_RETURN_TYPE_UINT
                              : D3D_RETURN_TYPE_SINT;
    }
  }

  else {
    type.TextureValue = D3D_RETURN_TYPE_MIXED;
  }

  switch (vectorParams[1].getAsIntegral().getZExtValue()) {
  case 2:
    type.RegisterFlags = (D3D_SHADER_INPUT_FLAGS)D3D_SIF_TEXTURE_COMPONENT_0;
    break;
  case 3:
    type.RegisterFlags = (D3D_SHADER_INPUT_FLAGS)D3D_SIF_TEXTURE_COMPONENT_1;
    break;
  case 4:
    type.RegisterFlags = (D3D_SHADER_INPUT_FLAGS)D3D_SIF_TEXTURE_COMPONENTS;
    break;
  }

  //Parse type

  if (TypeName == "Buffer") {
    type.TextureDimension = D3D_SRV_DIMENSION_BUFFER;
    return type;
  }

  bool isFeedback = false;

  if (TypeName.size() > 8 && TypeName.substr(0, 8) == "Feedback") {
    isFeedback = true;
    TypeName = TypeName.substr(8);
    type.RegisterType = D3D_SIT_UAV_FEEDBACKTEXTURE;
  }

  bool isArray = false;

  if (TypeName.size() > 5 && TypeName.substr(TypeName.size() - 5) == "Array") {
    isArray = true;
    TypeName = TypeName.substr(0, TypeName.size() - 5);
  }

  if (TypeName == "Texture2D")
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURE2D;

  else if (TypeName == "TextureCube")
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURECUBE;

  else if (TypeName == "Texture3D")
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURE3D;

  else if (TypeName == "Texture1D")
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURE1D;

  else if (TypeName == "Texture2DMS") {
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURE2DMS;
    type.SampleCount = 0;
  }

  if (isArray)      //Arrays are always 1 behind the regular type
    type.TextureDimension = (D3D_SRV_DIMENSION)(type.TextureDimension + 1);

  return type;
}

static DxcRegisterTypeInfo GetRegisterTypeInfo(ASTContext &ASTCtx,
                                               QualType Type) {

  QualType realType = Type.getDesugaredType(ASTCtx);
  const RecordType *RT = realType->getAs<RecordType>();
  assert(RT && "GetRegisterTypeInfo() type is not a RecordType");

  const CXXRecordDecl *recordDecl = RT->getAsCXXRecordDecl();
  assert(recordDecl && "GetRegisterTypeInfo() type is not a CXXRecordDecl");

  std::string typeName = recordDecl->getNameAsString();

  if (typeName.size() >= 17 &&
      typeName.substr(0, 17) == "RasterizerOrdered") {
    typeName = typeName.substr(17);
  }

  if (typeName == "SamplerState" || typeName == "SamplerComparisonState") {
    return {D3D_SIT_SAMPLER, typeName == "SamplerComparisonState"
                                 ? D3D_SIF_COMPARISON_SAMPLER
                                 : (D3D_SHADER_INPUT_FLAGS)0};
  }
  
  DxcRegisterTypeInfo info = {};

  if (const ClassTemplateSpecializationDecl *spec =
          dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {

    const ArrayRef<TemplateArgument> &array =
        spec->getTemplateArgs().asArray();

    if (array.size() == 1)
      info.SampleCount = (uint32_t) (ASTCtx.getTypeSize(array[0].getAsType()) / 8);
  }

  if (typeName == "AppendStructuredBuffer") {
    info.RegisterType = D3D_SIT_UAV_APPEND_STRUCTURED;
    return info;
  }

  if (typeName == "ConsumeStructuredBuffer") {
    info.RegisterType = D3D_SIT_UAV_CONSUME_STRUCTURED;
    return info;
  }

  if (typeName == "RaytracingAccelerationStructure") {
    info.RegisterType = D3D_SIT_RTACCELERATIONSTRUCTURE;
    info.SampleCount = (uint32_t)-1;
    return info;
  }

  if (typeName == "TextureBuffer") {
    info.RegisterType = D3D_SIT_TBUFFER;
    return info;
  }

  if (typeName == "ConstantBuffer") {
    info.RegisterType = D3D_SIT_CBUFFER;
    return info;
  }

  bool isWrite =
      typeName.size() > 2 && typeName[0] == 'R' && typeName[1] == 'W';

  if (isWrite)
    typeName = typeName.substr(2);

  if (typeName == "StructuredBuffer") {
    info.RegisterType =
        isWrite ? D3D_SIT_UAV_RWSTRUCTURED : D3D_SIT_STRUCTURED;
    return info;
  }

  if (typeName == "ByteAddressBuffer") {
    info.RegisterType =
        isWrite ? D3D_SIT_UAV_RWBYTEADDRESS : D3D_SIT_BYTEADDRESS;
    return info;
  }

  return GetTextureRegisterInfo(ASTCtx, typeName, isWrite, recordDecl);
}

static uint32_t PushArray(DxcReflectionData &Refl, uint32_t ArraySizeFlat,
                          const std::vector<uint32_t> &ArraySize) {

  if (ArraySizeFlat <= 1 || ArraySize.size() <= 1)
    return (uint32_t)-1;

  assert(Refl.Arrays.size() < (uint32_t)((1u << 31) - 1) && "Arrays would overflow");
  uint32_t arrayId = (uint32_t)Refl.Arrays.size();

  uint32_t arrayCountStart = (uint32_t)Refl.ArraySizes.size();
  uint32_t numArrayElements = std::min((size_t)8, ArraySize.size());
  assert(Refl.ArraySizes.size() + numArrayElements < ((1 << 28) - 1) &&
         "Array elements would overflow");

  for (uint32_t i = 0; i < ArraySize.size() && i < 8; ++i) {

    uint32_t arraySize = ArraySize[i];

    // Flatten rest of array to at least keep consistent array elements
    if (i == 7)
      for (uint32_t j = i + 1; j < ArraySize.size(); ++j)
        arraySize *= ArraySize[j];

    Refl.ArraySizes.push_back(arraySize);
  }

  DxcHLSLArray arr = {numArrayElements, arrayCountStart};

  for (uint32_t i = 0; i < Refl.Arrays.size(); ++i)
    if (Refl.Arrays[i] == arr)
      return i;

  Refl.Arrays.push_back(arr);
  return arrayId;
}

uint32_t GenerateTypeInfo(ASTContext &ASTCtx, DxcReflectionData &Refl,
                          QualType Original, bool DefaultRowMaj) {

  // Unwrap array

  uint32_t arraySize = 1;
  QualType underlying = Original, forName = Original;
  std::vector<uint32_t> arrayElem;

  while (const ConstantArrayType *arr =
             dyn_cast<ConstantArrayType>(underlying)) {
    uint32_t current = arr->getSize().getZExtValue();
    arrayElem.push_back(current);
    arraySize *= arr->getSize().getZExtValue();
    forName = arr->getElementType();
    underlying = forName.getCanonicalType();
  }

  underlying = underlying.getCanonicalType();

  // Name; Omit struct, class and const keywords

  PrintingPolicy policy(ASTCtx.getLangOpts());
  policy.SuppressScope = false;
  policy.AnonymousTagLocations = false;
  policy.SuppressTagKeyword = true; 

  uint32_t nameId = RegisterString(Refl, forName.getLocalUnqualifiedType().getAsString(policy));
  uint32_t arrayId = PushArray(Refl, arraySize, arrayElem);
  uint32_t elementsOrArrayId = 0;

  if (arrayId != (uint32_t)-1)
    elementsOrArrayId = (1u << 31) | arrayId;

  else
    elementsOrArrayId = arraySize > 1 ? arraySize : 0;

  //Unwrap vector and matrix

  D3D_SHADER_VARIABLE_CLASS cls = D3D_SVC_STRUCT;
  uint8_t rows = 0, columns = 0;

  uint32_t membersCount = 0;
  uint32_t membersOffset = 0;

  if (const RecordType *record = underlying->getAs<RecordType>()) {

     bool standardType = false;

    RecordDecl *recordDecl = record->getDecl();

    if (const ClassTemplateSpecializationDecl *templateClass =
            dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {

      const std::string &name = templateClass->getIdentifier()->getName();

      const ArrayRef<TemplateArgument> &params =
          templateClass->getTemplateArgs().asArray();

      uint32_t magic = 0;
      std::memcpy(&magic, name.c_str(), std::min(sizeof(magic), name.size()));

      std::string_view subs =
          name.size() < sizeof(magic)
              ? std::string_view()
              : std::string_view(name).substr(sizeof(magic));

      switch (magic) {

      case DXC_FOURCC('v', 'e', 'c', 't'):

        if (subs == "or") {

          rows = 1;

          assert(params.size() == 2 && !params[0].getAsType().isNull() &&
                 params[1].getKind() == TemplateArgument::Integral &&
                 "Expected vector to be vector<T, N>");

          underlying = params[0].getAsType();
          columns = params[1].getAsIntegral().getSExtValue();
          cls = D3D_SVC_VECTOR;
          standardType = true;
        }

        break;

      case DXC_FOURCC('m', 'a', 't', 'r'):

        if (subs == "ix") {

          assert(params.size() == 3 && !params[0].getAsType().isNull() &&
                 params[1].getKind() == TemplateArgument::Integral &&
                 params[2].getKind() == TemplateArgument::Integral &&
                 "Expected matrix to be matrix<T, R, C>");

          underlying = params[0].getAsType();
          columns = params[1].getAsIntegral().getSExtValue();
          rows = params[2].getAsIntegral().getSExtValue();

          bool isRowMajor = DefaultRowMaj;

          HasHLSLMatOrientation(Original, &isRowMajor);

          if (!isRowMajor)
            std::swap(rows, columns);

          cls = isRowMajor ? D3D_SVC_MATRIX_ROWS : D3D_SVC_MATRIX_COLUMNS;
          standardType = true;
        }

        break;
      }

      // TODO:
      //           D3D_SVT_TEXTURE	= 5,
      //  D3D_SVT_TEXTURE1D	= 6,
      //  D3D_SVT_TEXTURE2D	= 7,
      //  D3D_SVT_TEXTURE3D	= 8,
      //  D3D_SVT_TEXTURECUBE	= 9,
      //  D3D_SVT_SAMPLER	= 10,
      //  D3D_SVT_SAMPLER1D	= 11,
      //  D3D_SVT_SAMPLER2D	= 12,
      //  D3D_SVT_SAMPLER3D	= 13,
      //  D3D_SVT_SAMPLERCUBE	= 14,
      //  D3D_SVT_BUFFER	= 25,
      //  D3D_SVT_CBUFFER	= 26,
      //  D3D_SVT_TBUFFER	= 27,
      //  D3D_SVT_TEXTURE1DARRAY	= 28,
      //  D3D_SVT_TEXTURE2DARRAY	= 29,
      //  D3D_SVT_TEXTURE2DMS	= 32,
      //  D3D_SVT_TEXTURE2DMSARRAY	= 33,
      //  D3D_SVT_TEXTURECUBEARRAY	= 34,
      //  D3D_SVT_RWTEXTURE1D	= 40,
      //  D3D_SVT_RWTEXTURE1DARRAY	= 41,
      //  D3D_SVT_RWTEXTURE2D	= 42,
      //  D3D_SVT_RWTEXTURE2DARRAY	= 43,
      //  D3D_SVT_RWTEXTURE3D	= 44,
      //  D3D_SVT_RWBUFFER	= 45,
      //  D3D_SVT_BYTEADDRESS_BUFFER	= 46,
      //  D3D_SVT_RWBYTEADDRESS_BUFFER	= 47,
      //  D3D_SVT_STRUCTURED_BUFFER	= 48,
      //  D3D_SVT_RWSTRUCTURED_BUFFER	= 49,
      //  D3D_SVT_APPEND_STRUCTURED_BUFFER	= 50,
      //  D3D_SVT_CONSUME_STRUCTURED_BUFFER	= 51,
    }

    // Fill members

    if (!standardType && recordDecl->isCompleteDefinition()) {

      for (Decl *decl : recordDecl->decls()) {

        // TODO: We could query other types VarDecl

        FieldDecl *fieldDecl = dyn_cast<FieldDecl>(decl);

        if (!fieldDecl)
          continue;

        QualType original = fieldDecl->getType();
        std::string name = fieldDecl->getName();

        uint32_t nameId = RegisterString(Refl, name);
        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, original, DefaultRowMaj);

        if (!membersCount)
          membersOffset = (uint32_t) Refl.Members.size();

        assert(Refl.Members.size() <= (uint32_t)-1 && "Members out of bounds");
        Refl.Members.push_back(DxcHLSLMember{nameId, typeId});
        ++membersCount;
      }
    }
  }

  //Type name

  D3D_SHADER_VARIABLE_TYPE type = D3D_SVT_VOID;

  if (const BuiltinType *bt = dyn_cast<BuiltinType>(underlying)) {

    if (!rows)
      rows = columns = 1;

    if (cls == D3D_SVC_STRUCT)
      cls = D3D_SVC_SCALAR;

    switch (bt->getKind()) {

    case BuiltinType::Void:
      type = D3D_SVT_VOID;
      break;

    case BuiltinType::Min10Float:
      type = D3D_SVT_MIN10FLOAT;
      break;

    case BuiltinType::Min16Float:
      type = D3D_SVT_MIN16FLOAT;
      break;

    case BuiltinType::HalfFloat:
    case BuiltinType::Half:
      type = D3D_SVT_FLOAT16;
      break;

    case BuiltinType::Short:
      type = D3D_SVT_INT16;
      break;

    case BuiltinType::Min12Int:
      type = D3D_SVT_MIN12INT;
      break;

    case BuiltinType::Min16Int:
      type = D3D_SVT_MIN16INT;
      break;

    case BuiltinType::Min16UInt:
      type = D3D_SVT_MIN16UINT;
      break;

    case BuiltinType::UShort:
      type = D3D_SVT_UINT16;
      break;

    case BuiltinType::Float:
      type = D3D_SVT_FLOAT;
      break;

    case BuiltinType::Int:
      type = D3D_SVT_INT;
      break;

    case BuiltinType::UInt:
      type = D3D_SVT_UINT;
      break;

    case BuiltinType::Bool:
      type = D3D_SVT_BOOL;
      break;

    case BuiltinType::Double:
      type = D3D_SVT_DOUBLE;
      break;

    case BuiltinType::ULongLong:
      type = D3D_SVT_UINT64;
      break;

    case BuiltinType::LongLong:
      type = D3D_SVT_INT64;
      break;

    default:
      assert(false && "Invalid builtin type");
      break;
    }
  }

  //TODO: Base class

  uint32_t baseClass = (uint32_t) -1;

  assert(Refl.Types.size() < (uint32_t)-1 && "Type id out of bounds");

  DxcHLSLType hlslType(nameId, baseClass, elementsOrArrayId, cls, type, rows,
                   columns, membersCount, membersOffset);

  uint32_t i = 0, j = (uint32_t)Refl.Types.size();

  for (; i < j; ++i)
    if (Refl.Types[i] == hlslType)
      break;

  if (i == j)
    Refl.Types.push_back(hlslType);

  return i;
}

D3D_CBUFFER_TYPE GetBufferType(uint8_t Type) {

  switch (Type) {

  case D3D_SIT_CBUFFER:
    return D3D_CT_CBUFFER;

  case D3D_SIT_TBUFFER:
    return D3D_CT_TBUFFER;

  case D3D_SIT_STRUCTURED:
  case D3D_SIT_UAV_RWSTRUCTURED:
  case D3D_SIT_UAV_APPEND_STRUCTURED:
  case D3D_SIT_UAV_CONSUME_STRUCTURED:
  case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
    return D3D_CT_RESOURCE_BIND_INFO;

  default:
    return D3D_CT_INTERFACE_POINTERS;
  }
}

static void FillReflectionRegisterAt(
    const DeclContext &Ctx, ASTContext &ASTCtx, const SourceManager &SM,
    DiagnosticsEngine &Diag, QualType Type, uint32_t ArraySizeFlat,
    ValueDecl *ValDesc, const std::vector<uint32_t> &ArraySize,
    DxcReflectionData &Refl, uint32_t AutoBindingSpace, uint32_t ParentNodeId,
    bool DefaultRowMaj) {

  ArrayRef<hlsl::UnusualAnnotation *> UA = ValDesc->getUnusualAnnotations();

  hlsl::RegisterAssignment *reg = nullptr;

  for (auto It = UA.begin(), E = UA.end(); It != E; ++It) {

    if ((*It)->getKind() != hlsl::UnusualAnnotation::UA_RegisterAssignment)
      continue;

    reg = cast<hlsl::RegisterAssignment>(*It);
  }

  assert(reg && "Found a register missing a RegisterAssignment, even though "
                "GenerateConsistentBindings should have already generated it");

  DxcRegisterTypeInfo inputType = GetRegisterTypeInfo(ASTCtx, Type);

  uint32_t nodeId = PushNextNodeId(
      Refl, SM, ASTCtx.getLangOpts(), ValDesc->getName(), ValDesc,
      DxcHLSLNodeType::Register, ParentNodeId, (uint32_t)Refl.Registers.size());

  uint32_t arrayId = PushArray(Refl, ArraySizeFlat, ArraySize);

  uint32_t bufferId = 0;
  D3D_CBUFFER_TYPE bufferType = GetBufferType(inputType.RegisterType);
  
  if(bufferType != D3D_CT_INTERFACE_POINTERS) {
    bufferId = (uint32_t) Refl.Buffers.size();
    Refl.Buffers.push_back({bufferType, nodeId});
  }

  DxcHLSLRegister regD3D12 = {

      inputType.RegisterType,
      reg->RegisterNumber,
      ArraySizeFlat,
      (uint32_t)inputType.RegisterFlags,
      inputType.TextureValue,
      inputType.TextureDimension,
      inputType.SampleCount,
      reg->RegisterSpace.hasValue() ? reg->RegisterSpace.getValue()
                                    : AutoBindingSpace,
      nodeId,
      arrayId,
      bufferId
  };

  Refl.Registers.push_back(regD3D12);

  bool isListType = true;

  switch (inputType.RegisterType) {

  case D3D_SIT_CBUFFER:
  case D3D_SIT_TBUFFER:
    isListType = false;
    [[fallthrough]];

  case D3D_SIT_STRUCTURED:
  case D3D_SIT_UAV_RWSTRUCTURED:
  case D3D_SIT_UAV_APPEND_STRUCTURED:
  case D3D_SIT_UAV_CONSUME_STRUCTURED:
  case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: {

    const RecordType *recordType = Type->getAs<RecordType>();

    assert(recordType && "Invalid type (not RecordType)");

    const ClassTemplateSpecializationDecl *templateDesc =
        dyn_cast<ClassTemplateSpecializationDecl>(recordType->getDecl());

    assert(templateDesc && "Invalid template type");

    const ArrayRef<TemplateArgument> &params =
        templateDesc->getTemplateArgs().asArray();

    assert(params.size() == 1 && "Expected Type<T>");

    QualType innerType = params[0].getAsType();

    // The name of the inner struct is $Element if 'array', otherwise equal to
    // register name

    uint32_t typeId = GenerateTypeInfo(ASTCtx, Refl, innerType, DefaultRowMaj);

    SourceRange sourceRange = ValDesc->getSourceRange();

    uint32_t nestNodeId =
        PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(),
                       isListType ? "$Element" : ValDesc->getName(), nullptr,
                       DxcHLSLNodeType::Variable, nodeId, typeId, &sourceRange);

    break;
  }
  }
}

class PrintfStream : public llvm::raw_ostream {
public:
  PrintfStream() { SetUnbuffered(); }

private:
  void write_impl(const char *Ptr, size_t Size) override {
    printf("%.*s\n", (int)Size, Ptr); // Print the raw buffer directly
  }

  uint64_t current_pos() const override { return 0; }
};

template<typename T>
void RecurseBuffer(ASTContext &ASTCtx, const SourceManager &SM,
                   DxcReflectionData &Refl, const T& Decls,
                   bool DefaultRowMaj, uint32_t ParentId) {

  for (Decl *decl : Decls) {

    ValueDecl *valDecl = dyn_cast<ValueDecl>(decl);
    assert(valDecl && "Decl was expected to be a ValueDecl but wasn't");
    QualType original = valDecl->getType();

    std::string name = valDecl->getName();

    uint32_t typeId = GenerateTypeInfo(ASTCtx, Refl, original, DefaultRowMaj);

    uint32_t nodeId = PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, decl,
                   DxcHLSLNodeType::Variable, ParentId, typeId);

    //Handle struct recursion

    if (RecordDecl *recordDecl = dyn_cast<RecordDecl>(decl)) {

      if (!recordDecl->isCompleteDefinition())
        continue;

      RecurseBuffer(ASTCtx, SM, Refl, recordDecl->fields(), DefaultRowMaj, nodeId);
    }
  }
}

uint32_t RegisterBuffer(ASTContext &ASTCtx, DxcReflectionData &Refl,
                        const SourceManager &SM, DeclContext *Buffer,
                        uint32_t NodeId, D3D_CBUFFER_TYPE Type,
                        bool DefaultRowMaj) {

  assert(Refl.Buffers.size() < (uint32_t)-1 && "Buffer id out of bounds");
  uint32_t bufferId = (uint32_t)Refl.Buffers.size();

  RecurseBuffer(ASTCtx, SM, Refl, Buffer->decls(), DefaultRowMaj, NodeId);

  Refl.Buffers.push_back({Type, NodeId});

  return bufferId;
}

/*
static void AddFunctionParameters(ASTContext &ASTCtx, QualType Type, Decl *Decl,
                                  DxcReflectionData &Refl, const SourceManager &SM,
                                  uint32_t ParentNodeId) {

  PrintingPolicy printingPolicy(ASTCtx.getLangOpts());

  QualType desugared = Type.getDesugaredType(ASTCtx);

  PrintfStream str;
  desugared.print(str, printingPolicy);

  if (Decl)
    Decl->print(str);

  //Generate parameter

  uint32_t nodeId = PushNextNodeId(
      Refl, SM, ASTCtx.getLangOpts(),
      Decl && dyn_cast<NamedDecl>(Decl) ? dyn_cast<NamedDecl>(Decl)->getName()
                                        : "",
      Decl, DxcHLSLNodeType::Parameter, ParentNodeId,
      (uint32_t)Refl.Parameters.size());

  std::string semanticName;

  if (NamedDecl *ValDesc = dyn_cast<NamedDecl>(Decl)) {

    ArrayRef<hlsl::UnusualAnnotation *> UA = ValDesc->getUnusualAnnotations();

    for (auto It = UA.begin(), E = UA.end(); It != E; ++It) {

      if ((*It)->getKind() != hlsl::UnusualAnnotation::UA_SemanticDecl)
        continue;

      semanticName = cast<hlsl::SemanticDecl>(*It)->SemanticName;
    }
  }

  DxcHLSLParameter parameter{std::move(semanticName)};

  type, clss, rows, columns, interpolationMode, flags;
  parameter.NodeId = nodeId;

  Refl.Parameters.push_back(parameter);

  //It's a struct, add parameters recursively
}*/

enum ReflectionMask : uint32_t {
  ReflectionMask_None               = 0,
  ReflectionMask_Basics             = 1 << 0,     //Includes cbuffer and registers
  ReflectionMask_Functions          = 1 << 1,
  ReflectionMask_Namespaces         = 1 << 2,
  ReflectionMask_UserTypes          = 1 << 3,     //Include user types (struct, enum, typedef, etc.)
  ReflectionMask_FunctionInternals  = 1 << 4,     //Variables, structs, functions defined in functions
  ReflectionMask_Variables          = 1 << 5      //Variables not included in $Global or cbuffers
};

ReflectionMask& operator|=(ReflectionMask &a, ReflectionMask b) {
  return a = (ReflectionMask)((uint32_t)a | (uint32_t)b);
}

static void RecursiveReflectHLSL(const DeclContext &Ctx, ASTContext &ASTCtx,
                                 DiagnosticsEngine &Diags,
                                 const SourceManager &SM,
                                 DxcReflectionData &Refl,
                                 uint32_t AutoBindingSpace,
                                 uint32_t Depth,
                                 ReflectionMask InclusionFlags,
                                 uint32_t ParentNodeId, bool DefaultRowMaj) {

  PrintfStream pfStream;

  PrintingPolicy printingPolicy(ASTCtx.getLangOpts());

  printingPolicy.SuppressInitializers = true;
  printingPolicy.AnonymousTagLocations = false;
  printingPolicy.TerseOutput =
      true; // No inheritance list, trailing semicolons, etc.
  printingPolicy.PolishForDeclaration = true; // Makes it print as a decl
  printingPolicy.SuppressSpecifiers = false; // Prints e.g. "static" or "inline"
  printingPolicy.SuppressScope = true;

  // Traverse AST to grab reflection data

  //TODO: Niels, scopes (if/switch/for/empty scope)

  for (Decl *it : Ctx.decls()) {

    SourceLocation Loc = it->getLocation();
    if (Loc.isInvalid() || SM.isInSystemHeader(Loc))
      continue;

    if (HLSLBufferDecl *CBuffer = dyn_cast<HLSLBufferDecl>(it)) {

      if(!(InclusionFlags & ReflectionMask_Basics))
        continue;
      
      // TODO: Add for reflection even though it might not be important

      if (Depth != 0)
        continue;

      hlsl::RegisterAssignment *reg = nullptr;
      ArrayRef<hlsl::UnusualAnnotation *> UA = CBuffer->getUnusualAnnotations();

      for (auto It = UA.begin(), E = UA.end(); It != E; ++It) {

        if ((*It)->getKind() != hlsl::UnusualAnnotation::UA_RegisterAssignment)
          continue;

        reg = cast<hlsl::RegisterAssignment>(*It);
      }

      assert(reg &&
             "Found a cbuffer missing a RegisterAssignment, even though "
             "GenerateConsistentBindings should have already generated it");

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), CBuffer->getName(),
                         CBuffer, DxcHLSLNodeType::Register, ParentNodeId,
                         (uint32_t)Refl.Registers.size());
      
      uint32_t bufferId = RegisterBuffer(ASTCtx, Refl, SM, CBuffer, nodeId,
                                         D3D_CT_CBUFFER, DefaultRowMaj);

      DxcHLSLRegister regD3D12 = {

          D3D_SIT_CBUFFER,
          reg->RegisterNumber,
          1,
          (uint32_t) D3D_SIF_USERPACKED,
          (D3D_RESOURCE_RETURN_TYPE) 0,
          D3D_SRV_DIMENSION_UNKNOWN,
          0,
          reg->RegisterSpace.hasValue() ? reg->RegisterSpace.getValue()
                                        : AutoBindingSpace,
          nodeId,
          (uint32_t)-1,
          bufferId
      };

      Refl.Registers.push_back(regD3D12);
    }

    else if (FunctionDecl *Func = dyn_cast<FunctionDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_Functions))
        continue;

      const FunctionDecl *Definition = nullptr;

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), Func->getName(), Func,
                         DxcHLSLNodeType::Function, ParentNodeId,
                         (uint32_t)Refl.Functions.size());

      bool hasDefinition = Func->hasBody(Definition);
      DxcHLSLFunction func = {nodeId, Func->getNumParams(),
                              !Func->getReturnType().getTypePtr()->isVoidType(),
                              hasDefinition};

      /*
      for (uint32_t i = 0; i < func.NumParameters; ++i)
        AddFunctionParameters(ASTCtx, Func->getParamDecl(i)->getType(),
                              Func->getParamDecl(i), Refl, SM, nodeId);

      if (func.HasReturn)
        AddFunctionParameters(ASTCtx, Func->getReturnType(), nullptr, Refl, SM,
                              nodeId);*/

      Refl.Functions.push_back(std::move(func));

      if (hasDefinition && (InclusionFlags & ReflectionMask_FunctionInternals)) {
        RecursiveReflectHLSL(*Definition, ASTCtx, Diags, SM, Refl,
                             AutoBindingSpace, Depth + 1, InclusionFlags,
                             nodeId, DefaultRowMaj);
      }
    }

    else if (FieldDecl *Field = dyn_cast<FieldDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_UserTypes))
        continue;

      //Field->print(pfStream, printingPolicy);
    }

    else if (TypedefDecl *Typedef = dyn_cast<TypedefDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_UserTypes))
        continue;

      // Typedef->print(pfStream, printingPolicy);
    }

    else if (TypeAliasDecl *TypeAlias = dyn_cast<TypeAliasDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_UserTypes))
        continue;

      // TypeAlias->print(pfStream, printingPolicy);
    }

    else if (EnumDecl *Enum = dyn_cast<EnumDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_UserTypes))
        continue;

      uint32_t nodeId = PushNextNodeId(
          Refl, SM, ASTCtx.getLangOpts(), Enum->getName(), Enum,
          DxcHLSLNodeType::Enum, ParentNodeId, (uint32_t)Refl.Enums.size());

      for (EnumConstantDecl *EnumValue : Enum->enumerators()) {

        uint32_t childNodeId =
            PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), EnumValue->getName(),
                           EnumValue, DxcHLSLNodeType::EnumValue, nodeId,
                           (uint32_t)Refl.EnumValues.size());

        Refl.EnumValues.push_back(
            {EnumValue->getInitVal().getSExtValue(), childNodeId});
      }

      assert(Refl.EnumValues.size() < (uint32_t)(1 << 30) &&
             "Enum values overflow");

      QualType enumType = Enum->getIntegerType();
      QualType desugared = enumType.getDesugaredType(ASTCtx);
      const auto &semantics = ASTCtx.getTypeInfo(desugared);

      D3D12_HLSL_ENUM_TYPE type;

      switch (semantics.Width) {

      default:
      case 32:
        type = desugared->isUnsignedIntegerType() ? D3D12_HLSL_ENUM_TYPE_UINT
                                                  : D3D12_HLSL_ENUM_TYPE_INT;
        break;

      case 16:
        type = desugared->isUnsignedIntegerType()
                   ? D3D12_HLSL_ENUM_TYPE_UINT16_T
                   : D3D12_HLSL_ENUM_TYPE_INT16_T;
        break;

      case 64:
        type = desugared->isUnsignedIntegerType()
                   ? D3D12_HLSL_ENUM_TYPE_UINT64_T
                   : D3D12_HLSL_ENUM_TYPE_INT64_T;
        break;
      }

      Refl.Enums.push_back({nodeId, type});
    }

    else if (ValueDecl *ValDecl = dyn_cast<ValueDecl>(it)) {

      if(!(InclusionFlags & ReflectionMask_Basics))
        continue;

      //TODO: Handle values

      //ValDecl->print(pfStream);

      uint32_t arraySize = 1;
      QualType type = ValDecl->getType();
      std::vector<uint32_t> arrayElem;

      while (const ConstantArrayType *arr =
              dyn_cast<ConstantArrayType>(type)) {
        uint32_t current = arr->getSize().getZExtValue();
        arrayElem.push_back(current);
        arraySize *= arr->getSize().getZExtValue();
        type = arr->getElementType();
      }

      if (!IsHLSLResourceType(type))
        continue;

      // TODO: Add for reflection even though it might not be important

      if (Depth != 0)
        continue;

      FillReflectionRegisterAt(Ctx, ASTCtx, SM, Diags, type, arraySize, ValDecl,
                               arrayElem, Refl, AutoBindingSpace, ParentNodeId,
                               DefaultRowMaj);
    }

    else if (RecordDecl *RecDecl = dyn_cast<RecordDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_UserTypes))
        continue;

      //RecDecl->print(pfStream, printingPolicy);
      /*RecursiveReflectHLSL(*RecDecl, ASTCtx, Diags, SM, Refl,
                           AutoBindingSpace, Depth + 1, InclusionFlags, nodeId);*/
    }

    else if (NamespaceDecl *Namespace = dyn_cast<NamespaceDecl>(it)) {

      if (!(InclusionFlags & ReflectionMask_Namespaces))
        continue;

      uint32_t nodeId = PushNextNodeId(
          Refl, SM, ASTCtx.getLangOpts(), Namespace->getName(), Namespace,
          DxcHLSLNodeType::Namespace, ParentNodeId, 0);

      RecursiveReflectHLSL(*Namespace, ASTCtx, Diags, SM, Refl,
                           AutoBindingSpace, Depth + 1, InclusionFlags, nodeId,
                           DefaultRowMaj);
    }
  }
}

static void ReflectHLSL(ASTHelper &astHelper, DxcReflectionData& Refl,
                        uint32_t AutoBindingSpace, ReflectionMask ReflectMask,
                        bool DefaultRowMaj) {

  TranslationUnitDecl &Ctx = *astHelper.tu;
  DiagnosticsEngine &Diags = Ctx.getParentASTContext().getDiagnostics();
  const SourceManager &SM = astHelper.compiler.getSourceManager();

  Refl.Strings.push_back("");
  Refl.StringsToId[""] = 0;

  Refl.Nodes.push_back(DxcHLSLNode{0, DxcHLSLNodeType::Namespace, 0, 0, 0, 0,
                                   0xFFFF, 0, 0, 0, 0, 0});

  RecursiveReflectHLSL(Ctx, astHelper.compiler.getASTContext(), Diags, SM, Refl,
                       AutoBindingSpace, 0, ReflectMask, 0, DefaultRowMaj);
}

static void GlobalVariableAsExternByDefault(DeclContext &Ctx) {
  for (auto it = Ctx.decls_begin(); it != Ctx.decls_end();) {
    auto cur = it++;
    if (VarDecl *VD = dyn_cast<VarDecl>(*cur)) {
      bool isInternal =
          VD->getStorageClass() == SC_Static || VD->isInAnonymousNamespace();
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

char RegisterGetSpaceChar(const DxcHLSLRegister &reg) {

  switch (reg.Type) {

  case D3D_SIT_UAV_RWTYPED:
  case D3D_SIT_UAV_RWSTRUCTURED:
  case D3D_SIT_UAV_RWBYTEADDRESS:
  case D3D_SIT_UAV_APPEND_STRUCTURED:
  case D3D_SIT_UAV_CONSUME_STRUCTURED:
  case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
  case D3D_SIT_UAV_FEEDBACKTEXTURE:
    return 'u';

  case D3D_SIT_CBUFFER:
    return 'b';

  case D3D_SIT_SAMPLER:
    return 's';

  default:
    return 't';
  }
}

std::string RegisterGetArraySize(const DxcReflectionData &Refl, const DxcHLSLRegister &reg) {

  if (reg.ArrayId != (uint32_t)-1) {

    DxcHLSLArray arr = Refl.Arrays[reg.ArrayId];
    std::string str;

    for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
      str += "[" + std::to_string(Refl.ArraySizes[arr.ArrayStart() + i]) + "]";

    return str;
  }

  return reg.BindCount > 1 ? "[" + std::to_string(reg.BindCount) + "]" : "";
}

std::string EnumTypeToString(D3D12_HLSL_ENUM_TYPE type) {

  static const char *arr[] = {
      "uint", "int", "uint64_t", "int64_t", "uint16_t", "int16_t",
  };

  return arr[type];
}

std::string NodeTypeToString(DxcHLSLNodeType type) {

  static const char *arr[] = {"Register",  "Function", "Enum",  "EnumValue",
                              "Namespace", "Typedef",  "Using", "Variable",
                              "Parameter", "Type"};

  return arr[(int)type];
}

std::string GetBuiltinTypeName(const DxcReflectionData &Refl,
                               const DxcHLSLType &Type) {

  std::string type;

  if (Type.Class != D3D_SVC_STRUCT) {

    static const char *arr[] = {"void",
                                "bool",
                                "int",
                                "float",
                                "string",
                                NULL,
                                "Texture1D",
                                "Texture2D",
                                "Texture3D",
                                "TextureCube",
                                "SamplerState",
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                "uint",
                                "uint8_t",
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                "Buffer",
                                "ConstantBuffer",
                                NULL,
                                "Texture1DArray",
                                "Texture2DArray",
                                NULL,
                                NULL,
                                "Texture2DMS",
                                "Texture2DMSArray",
                                "TextureCubeArray",
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                "double",
                                "RWTexture1D",
                                "RWTexture1DArray",
                                "RWTexture2D",
                                "RWTexture2DArray",
                                "RWTexture3D",
                                "RWBuffer",
                                "ByteAddressBuffer",
                                "RWByteAddressBuffer",
                                "StructuredBuffer",
                                "RWStructuredBuffer",
                                "AppendStructuredBuffer",
                                "ConsumeStructuredBuffer",
                                "min8float",
                                "min10float",
                                "min16float",
                                "min12int",
                                "min16int",
                                "min16uint",
                                "int16_t",
                                "uint16_t",
                                "float16_t",
                                "int64_t",
                                "uint64_t"};

    const char *ptr = arr[Type.Type];

    if (ptr)
      type = ptr;
  }

  switch (Type.Class) {

  case D3D_SVC_MATRIX_ROWS:
  case D3D_SVC_VECTOR:

    type += std::to_string(Type.Columns);

    if (Type.Class == D3D_SVC_MATRIX_ROWS)
      type += "x" + std::to_string(Type.Rows);

    break;

  case D3D_SVC_MATRIX_COLUMNS:
    type += std::to_string(Type.Rows) + "x" + std::to_string(Type.Columns);
    break;
  }

  return type;
}

std::string PrintTypeInfo(const DxcReflectionData &Refl,
                          const DxcHLSLType &Type,
                          const std::string &PreviousTypeName) {

  std::string result;

  if (Type.IsMultiDimensionalArray()) {

    const DxcHLSLArray &arr = Refl.Arrays[Type.GetMultiDimensionalArrayId()];

    for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
      result +=
          "[" + std::to_string(Refl.ArraySizes[arr.ArrayStart() + i]) + "]";

  }

  else if (Type.IsArray())
    result += "[" + std::to_string(Type.Get1DElements()) + "]";

  // Obtain type name (returns empty if it's not a builtin type)

  std::string underlyingTypeName = GetBuiltinTypeName(Refl, Type);

  if (PreviousTypeName != underlyingTypeName && underlyingTypeName.size())
    result += " (" + underlyingTypeName + ")";

  if (Type.BaseClass != (uint32_t)-1)
    result += " : " + Refl.Strings[Refl.Types[Type.BaseClass].NameId];

  return result;
}

void RecursePrintType(const DxcReflectionData &Refl, uint32_t TypeId,
                      uint32_t Depth, const char *Prefix = "") {

  const DxcHLSLType &type = Refl.Types[TypeId];

  const std::string &name = Refl.Strings[type.NameId];

  printf("%s%s%s%s\n", std::string(Depth, '\t').c_str(), Prefix, name.c_str(),
         PrintTypeInfo(Refl, type, name).c_str());

  for (uint32_t i = 0; i < type.MembersCount; ++i) {
    const DxcHLSLMember &member = Refl.Members[type.MembersStart + i];
    std::string prefix = Refl.Strings[member.NameId] + ": ";
    RecursePrintType(Refl, member.TypeId, Depth + 1, prefix.c_str());
  }
}

uint32_t RecursePrint(const DxcReflectionData &Refl, uint32_t NodeId,
                      uint32_t Depth, uint32_t IndexInParent) {

  const DxcHLSLNode &node = Refl.Nodes[NodeId];

  uint32_t typeToPrint = (uint32_t)-1;

  if (NodeId) {

    printf("%s%s %s\n", std::string(Depth - 1, '\t').c_str(),
           NodeTypeToString(node.GetNodeType()).c_str(),
           Refl.Strings[node.NameId].c_str());

    for (uint32_t i = 0; i < node.GetAnnotationCount(); ++i)
      printf("%s[[%s]]\n", std::string(Depth, '\t').c_str(),
             Refl.Strings[Refl.Annotations[node.GetAnnotationStart() + i]]
                 .c_str());

    uint32_t localId = node.GetLocalId();

    switch (node.GetNodeType()) {
        
    case DxcHLSLNodeType::Register: {
      const DxcHLSLRegister &reg = Refl.Registers[localId];
      printf("%s%s : register(%c%u, space%u);\n",
             std::string(Depth, '\t').c_str(),
             RegisterGetArraySize(Refl, reg).c_str(), RegisterGetSpaceChar(reg),
             reg.BindPoint, reg.Space);
      break;
    }
        
    case DxcHLSLNodeType::Variable:
      typeToPrint = localId;
      break;

    case DxcHLSLNodeType::Function: {
      const DxcHLSLFunction &func = Refl.Functions[localId];
      printf("%sreturn: %s, hasDefinition: %s, numParams: %u\n",
             std::string(Depth, '\t').c_str(),
             func.HasReturn() ? "true" : "false",
             func.HasDefinition() ? "true" : "false", func.GetNumParameters());

      break;
    }

    case DxcHLSLNodeType::Enum:
      printf("%s: %s\n", std::string(Depth, '\t').c_str(),
             EnumTypeToString(Refl.Enums[localId].Type).c_str());
      break;

    case DxcHLSLNodeType::EnumValue: {
      printf("%s#%u = %" PRIi64 "\n", std::string(Depth, '\t').c_str(),
             IndexInParent, Refl.EnumValues[localId].Value);
        break;
    }

    //TODO:
    case DxcHLSLNodeType::Type:
    case DxcHLSLNodeType::Typedef:
    case DxcHLSLNodeType::Using:
    case DxcHLSLNodeType::Parameter:
        break;

    case DxcHLSLNodeType::Namespace:
    default:
      break;
    }
  }

  if (typeToPrint != (uint32_t)-1)
    RecursePrintType(Refl, typeToPrint, Depth);

  for (uint32_t i = 0, j = 0; i < node.GetChildCount(); ++i, ++j)
    i += RecursePrint(Refl, NodeId + 1 + i, Depth + 1, j);

  return node.GetChildCount();
}

struct DxcHLSLHeader {

  uint32_t MagicNumber;
  uint16_t Version;
  uint16_t Sources;

  uint32_t Strings;
  uint32_t Nodes;

  uint32_t Registers;
  uint32_t Functions;

  uint32_t Enums;
  uint32_t EnumValues;

  uint32_t Annotations;
  uint32_t Arrays;

  uint32_t ArraySizes;
  uint32_t Members;

  uint32_t Types;
  uint32_t Buffers;
};

template <typename T>
T &UnsafeCast(std::vector<std::byte> &Bytes, uint64_t Offset) {
  return *(T *)(Bytes.data() + Offset);
}

template <typename T>
const T &UnsafeCast(const std::vector<std::byte> &Bytes, uint64_t Offset) {
  return *(const T *)(Bytes.data() + Offset);
}

template <typename T>
void SkipPadding(uint64_t& Offset) {
  Offset = (Offset + alignof(T) - 1) / alignof(T) * alignof(T);
}

template <typename T>
void Skip(uint64_t& Offset, const std::vector<T>& Vec) {
  Offset += Vec.size() * sizeof(T);
}

template <typename T>
void Advance(uint64_t& Offset, const std::vector<T>& Vec) {
  SkipPadding<T>(Offset);
  Skip(Offset, Vec);
}

template <>
void Advance<std::string>(uint64_t &Offset,
                          const std::vector<std::string> &Vec) {
  for (const std::string &str : Vec) {
    Offset += str.size() >= 128 ? 2 : 1;
    Offset += str.size();
  }
}

template <typename T, typename T2, typename ...args>
void Advance(uint64_t& Offset, const std::vector<T>& Vec, const std::vector<T2>& Vec2, args... arg) {
  Advance(Offset, Vec);
  Advance(Offset, Vec2, arg...);
}

template<typename T>
void Append(std::vector<std::byte> &Bytes, uint64_t &Offset,
            const std::vector<T> &Vec) {
  static_assert(std::is_pod_v<T>, "Append only works on POD types");
  SkipPadding<T>(Offset);
  std::memcpy(&UnsafeCast<uint8_t>(Bytes, Offset), Vec.data(),
              Vec.size() * sizeof(T));
  Skip(Offset, Vec);
}

template <>
void Append<std::string>(std::vector<std::byte> &Bytes, uint64_t &Offset,
                         const std::vector<std::string> &Vec) {

  for (const std::string &str : Vec) {

    if (str.size() >= 128) {
      UnsafeCast<uint8_t>(Bytes, Offset++) =
          (uint8_t)(str.size() & 0x7F) | 0x80;
      UnsafeCast<uint8_t>(Bytes, Offset++) = (uint8_t)(str.size() >> 7);
    }

    else
      UnsafeCast<uint8_t>(Bytes, Offset++) = (uint8_t)str.size();

    std::memcpy(&UnsafeCast<char>(Bytes, Offset), str.data(), str.size());
    Offset += str.size();
  }
}

template <typename T, typename T2, typename ...args>
void Append(std::vector<std::byte> &Bytes, uint64_t &Offset,
            const std::vector<T> &Vec, const std::vector<T2> &Vec2,
            args... arg) {
  Append(Bytes, Offset, Vec);
  Append(Bytes, Offset, Vec2, arg...);
}

template <typename T, typename = std::enable_if_t<std::is_pod_v<T>>>
void Consume(const std::vector<std::byte> &Bytes, uint64_t &Offset, T &t) {

  static_assert(std::is_pod_v<T>, "Consume only works on POD types");

  SkipPadding<T>(Offset);

  if (Offset + sizeof(T) > Bytes.size())
    throw std::out_of_range("Couldn't consume; out of bounds!");

  std::memcpy(&t, &UnsafeCast<uint8_t>(Bytes, Offset), sizeof(T));
  Offset += sizeof(T);
}

template <typename T>
void Consume(const std::vector<std::byte> &Bytes, uint64_t &Offset, T *target,
             uint64_t Len) {

  static_assert(std::is_pod_v<T>, "Consume only works on POD types");

  SkipPadding<T>(Offset);

  if (Offset + sizeof(T) * Len > Bytes.size())
    throw std::out_of_range("Couldn't consume; out of bounds!");

  std::memcpy(target, &UnsafeCast<uint8_t>(Bytes, Offset), sizeof(T) * Len);
  Offset += sizeof(T) * Len;
}

template <typename T>
void Consume(const std::vector<std::byte> &Bytes, uint64_t &Offset,
            std::vector<T> &Vec, uint64_t Len) {
  Vec.resize(Len);
  Consume(Bytes, Offset, Vec.data(), Len);
}

template <>
void Consume<std::string>(const std::vector<std::byte> &Bytes, uint64_t &Offset,
                          std::vector<std::string> &Vec, uint64_t Len) {
  Vec.resize(Len);

  for (uint64_t i = 0; i < Len; ++i) {

      if (Offset >= Bytes.size())
        throw std::out_of_range("Couldn't consume string len; out of bounds!");
  
      uint16_t ourLen = uint8_t(Bytes.at(Offset++));

      if (ourLen >> 7) {

        if (Offset >= Bytes.size())
          throw std::out_of_range("Couldn't consume string len; out of bounds!");

        ourLen &= ~(1 << 7);
        ourLen |= uint16_t(Bytes.at(Offset++)) << 7;
      }

      if (Offset + ourLen > Bytes.size())
        throw std::out_of_range("Couldn't consume string len; out of bounds!");

      Vec[i].resize(ourLen);
      std::memcpy(Vec[i].data(), Bytes.data() + Offset, ourLen);
      Offset += ourLen;
  }
}

template <typename T, typename T2, typename ...args>
void Consume(const std::vector<std::byte> &Bytes, uint64_t &Offset,
             std::vector<T> &Vec, uint64_t Len, std::vector<T2> &Vec2,
             uint64_t Len2, args&... arg) {
  Consume(Bytes, Offset, Vec, Len);
  Consume(Bytes, Offset, Vec2, Len2, arg...);
}

static constexpr uint32_t DxcReflectionDataMagic = DXC_FOURCC('D', 'H', 'R', 'D');
static constexpr uint16_t DxcReflectionDataVersion = 0;

void DxcReflectionData::Dump(std::vector<std::byte> &Bytes) const {

  uint64_t toReserve = sizeof(DxcHLSLHeader);

  Advance(toReserve, Strings, Sources, Nodes, Registers, Functions, Enums,
          EnumValues, Annotations, ArraySizes, Members, Types, Buffers);

  Bytes.resize(toReserve);

  toReserve = 0;

  UnsafeCast<DxcHLSLHeader>(Bytes, toReserve) = {
      DxcReflectionDataMagic,      DxcReflectionDataVersion,
      uint16_t(Sources.size()),    uint32_t(Strings.size()),
      uint32_t(Nodes.size()),      uint32_t(Registers.size()),
      uint32_t(Functions.size()),  uint32_t(Enums.size()),
      uint32_t(EnumValues.size()), uint32_t(Annotations.size()),
      uint32_t(Arrays.size()),     uint32_t(ArraySizes.size()),
      uint32_t(Members.size()),    uint32_t(Types.size()),
      uint32_t(Buffers.size())};

  toReserve += sizeof(DxcHLSLHeader);

  Append(Bytes, toReserve, Strings, Sources, Nodes, Registers, Functions, Enums, EnumValues, Annotations,
          ArraySizes, Members, Types, Buffers);
}

DxcReflectionData::DxcReflectionData(const std::vector<std::byte> &Bytes) {

  uint64_t off = 0;
  DxcHLSLHeader header;
  Consume<DxcHLSLHeader>(Bytes, off, header);

  if (header.MagicNumber != DxcReflectionDataMagic)
    throw std::invalid_argument("Invalid magic number");

  if (header.Version != DxcReflectionDataVersion)
    throw std::invalid_argument("Unrecognized version number");

  Consume(Bytes, off, Strings, header.Strings, Sources, header.Sources, Nodes, header.Nodes, Registers,
          header.Registers, Functions, header.Functions, Enums, header.Enums,
          EnumValues, header.EnumValues, Annotations, header.Annotations, ArraySizes, header.ArraySizes, Members,
          header.Members, Types, header.Types, Buffers, header.Buffers);

  //Validation errors are throws to prevent accessing invalid data

  for(uint32_t i = 0; i < header.Sources; ++i)
    if(Sources[i] >= header.Strings)
      throw std::invalid_argument("Source path out of bounds");

  for(uint32_t i = 0; i < header.Nodes; ++i) {

    const DxcHLSLNode &node = Nodes[i];

    if(
      node.NameId >= header.Strings || 
      node.FileNameId >= header.Sources ||
      node.GetAnnotationStart() + node.GetAnnotationCount() > header.Annotations ||
      node.GetNodeType() > DxcHLSLNodeType::End ||
      (i && node.GetParentId() >= i) ||
      i + node.GetChildCount() > header.Nodes
    )
      throw std::invalid_argument("Node " + std::to_string(i) + " is invalid");

    uint32_t maxValue = 1;

    switch(node.GetNodeType()) {
      case DxcHLSLNodeType::Register:
        maxValue = header.Registers;
        break;
      case DxcHLSLNodeType::Function:
        maxValue = header.Functions;
        break;
      case DxcHLSLNodeType::Enum:
        maxValue = header.Enums;
        break;
      case DxcHLSLNodeType::EnumValue:
        maxValue = header.EnumValues;
        break;
      case DxcHLSLNodeType::Typedef:
      case DxcHLSLNodeType::Using:
      case DxcHLSLNodeType::Type:
      case DxcHLSLNodeType::Variable:
        maxValue = header.Types;
        break;
      case DxcHLSLNodeType::Parameter:
        throw std::invalid_argument("Node " + std::to_string(i) + " has unsupported 'parameter' type");
    }

    if(node.GetLocalId() >= maxValue)
      throw std::invalid_argument("Node " + std::to_string(i) + " has invalid localId");
  }

  for(uint32_t i = 0; i < header.Registers; ++i) {

    const DxcHLSLRegister &reg = Registers[i];

    if(
      reg.NodeId >= header.Nodes || 
      Nodes[reg.NodeId].GetNodeType() != DxcHLSLNodeType::Register ||
      Nodes[reg.NodeId].GetLocalId() != i
    )
      throw std::invalid_argument("Register " + std::to_string(i) + " points to an invalid nodeId");

    if (reg.Type > D3D_SIT_UAV_FEEDBACKTEXTURE ||
        reg.ReturnType > D3D_RETURN_TYPE_CONTINUED ||
        reg.Dimension > D3D_SRV_DIMENSION_BUFFEREX || !reg.BindCount ||
        (reg.ArrayId != uint32_t(-1) && reg.ArrayId >= header.Arrays) ||
        (reg.ArrayId != uint32_t(-1) && reg.BindCount <= 1))
      throw std::invalid_argument(
          "Register " + std::to_string(i) +
          " invalid type, returnType, bindCount, array or dimension");
    
    D3D_CBUFFER_TYPE bufferType = GetBufferType(reg.Type);

    if(bufferType != D3D_CT_INTERFACE_POINTERS) {

      if (reg.BufferId >= header.Buffers ||
          Buffers[reg.BufferId].NodeId != reg.NodeId ||
          Buffers[reg.BufferId].Type != bufferType)
          throw std::invalid_argument("Register " + std::to_string(i) +
                                      " invalid buffer referenced by register");
    }
  }

  for(uint32_t i = 0; i < header.Functions; ++i) {
    
    const DxcHLSLFunction &func = Functions[i];

    if (func.NodeId >= header.Nodes ||
        Nodes[func.NodeId].GetNodeType() != DxcHLSLNodeType::Function ||
        Nodes[func.NodeId].GetLocalId() != i)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " points to an invalid nodeId");
  }

  for(uint32_t i = 0; i < header.Enums; ++i) {
    
    const DxcHLSLEnumDesc &enm = Enums[i];

    if (enm.NodeId >= header.Nodes ||
        Nodes[enm.NodeId].GetNodeType() != DxcHLSLNodeType::Enum ||
        Nodes[enm.NodeId].GetLocalId() != i)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " points to an invalid nodeId");

    if (enm.Type < D3D12_HLSL_ENUM_TYPE_START ||
        enm.Type > D3D12_HLSL_ENUM_TYPE_END)
      throw std::invalid_argument("Enum " + std::to_string(i) +
                                  " has an invalid type");

    const DxcHLSLNode &node = Nodes[enm.NodeId];

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {
    
        const DxcHLSLNode &child = Nodes[enm.NodeId + 1 + j];

        if (child.GetChildCount() != 0 ||
            child.GetNodeType() != DxcHLSLNodeType::EnumValue)
          throw std::invalid_argument("Enum " + std::to_string(i) +
                                      " has an invalid enum value");
    }
  }

  for(uint32_t i = 0; i < header.EnumValues; ++i) {
    
    const DxcHLSLEnumValue &enumVal = EnumValues[i];

    if (enumVal.NodeId >= header.Nodes ||
        Nodes[enumVal.NodeId].GetNodeType() != DxcHLSLNodeType::EnumValue ||
        Nodes[enumVal.NodeId].GetLocalId() != i ||
        Nodes[Nodes[enumVal.NodeId].GetParentId()].GetNodeType() !=
            DxcHLSLNodeType::Enum)
      throw std::invalid_argument("Enum " + std::to_string(i) +
                                  " points to an invalid nodeId");
  }

  for (uint32_t i = 0; i < header.Arrays; ++i) {

    const DxcHLSLArray &arr = Arrays[i];

    if (arr.ArrayElem() <= 1 || arr.ArrayElem() > 8 ||
        arr.ArrayStart() + arr.ArrayElem() > header.ArraySizes)
      throw std::invalid_argument("Array " + std::to_string(i) +
                                  " points to an invalid array element");
  }

  for (uint32_t i = 0; i < header.Members; ++i) {

    const DxcHLSLMember &mem = Members[i];

    if (mem.NameId >= header.Strings || mem.TypeId >= header.Types)
      throw std::invalid_argument("Member " + std::to_string(i) +
                                  " points to an invalid string or type");
  }

  for (uint32_t i = 0; i < header.Annotations; ++i)
    if (Annotations[i] >= header.Strings)
      throw std::invalid_argument("Annotation " + std::to_string(i) +
                                  " points to an invalid string");

  for (uint32_t i = 0; i < header.Buffers; ++i) {

    const DxcHLSLBuffer &buf = Buffers[i];

    if (buf.NodeId >= header.Nodes ||
        Nodes[buf.NodeId].GetNodeType() != DxcHLSLNodeType::Register ||
        Nodes[buf.NodeId].GetLocalId() >= header.Registers ||
        Registers[Nodes[buf.NodeId].GetLocalId()].BufferId != i)
      throw std::invalid_argument("Buffer " + std::to_string(i) +
                                  " points to an invalid nodeId");

    const DxcHLSLNode &node = Nodes[buf.NodeId];

    if (!node.GetChildCount())
      throw std::invalid_argument("Buffer " + std::to_string(i) +
                                  " requires at least one Variable child");

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const DxcHLSLNode &child = Nodes[buf.NodeId + 1 + j];

      if (child.GetChildCount() != 0 ||
          child.GetNodeType() != DxcHLSLNodeType::Variable)
          throw std::invalid_argument("Buffer " + std::to_string(i) +
                                      " has to have only Variable child nodes");
    }
  }

  for (uint32_t i = 0; i < header.Members; ++i) {

    const DxcHLSLMember &mem = Members[i];

    if (mem.NameId >= header.Strings || mem.TypeId >= header.Types)
      throw std::invalid_argument("Member " + std::to_string(i) +
                                  " points to an invalid string or type");
  }
  
  for (uint32_t i = 0; i < header.Types; ++i) {

    const DxcHLSLType &type = Types[i];

    if (type.NameId >= header.Strings ||
        (type.BaseClass != uint32_t(-1) && type.BaseClass >= header.Types) ||
        (uint64_t)type.MembersStart + type.MembersCount > header.Members ||
        (type.ElementsOrArrayId >> 31 &&
         (type.ElementsOrArrayId << 1 >> 1) >= header.Arrays))
      throw std::invalid_argument(
          "Type " + std::to_string(i) +
          " points to an invalid string, base class or member");

    switch (type.Class) {

    case D3D_SVC_SCALAR:

      if (type.Columns != 1)
          throw std::invalid_argument("Type (scalar) " + std::to_string(i) +
                                      " should have columns == 1");

      [[fallthrough]];

    case D3D_SVC_VECTOR:

      if (type.Rows != 1)
          throw std::invalid_argument("Type (scalar/vector) " +
                                      std::to_string(i) +
                                      " should have rows == 1");

      [[fallthrough]];

    case D3D_SVC_MATRIX_ROWS:
    case D3D_SVC_MATRIX_COLUMNS:

        if (!type.Rows || !type.Columns || type.Rows > 128 || type.Columns > 128)
          throw std::invalid_argument("Type (scalar/vector/matrix) " +
                                      std::to_string(i) +
                                      " has invalid rows or columns");
        
        switch (type.Type) {
        case D3D_SVT_BOOL:
        case D3D_SVT_INT:
        case D3D_SVT_FLOAT:
        case D3D_SVT_MIN8FLOAT:
        case D3D_SVT_MIN10FLOAT:
        case D3D_SVT_MIN16FLOAT:
        case D3D_SVT_MIN12INT:
        case D3D_SVT_MIN16INT:
        case D3D_SVT_MIN16UINT:
        case D3D_SVT_INT16:
        case D3D_SVT_UINT16:
        case D3D_SVT_FLOAT16:
        case D3D_SVT_INT64:
        case D3D_SVT_UINT64:
        case D3D_SVT_UINT:
        case D3D_SVT_DOUBLE:
          break;

        default:
          throw std::invalid_argument("Type (scalar/matrix/vector) " +
                                      std::to_string(i) +
                                      " is of invalid type");
        }

        break;

    case D3D_SVC_STRUCT:

        if (!type.MembersCount)
          throw std::invalid_argument("Type (struct) " + std::to_string(i) +
                                      " is missing children");
        if (type.Type)
          throw std::invalid_argument("Type (struct) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

        if (type.Rows || type.Columns)
          throw std::invalid_argument("Type (struct) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

        break;

    case D3D_SVC_OBJECT:

        switch (type.Type) {
            
        case D3D_SVT_STRING:
        case D3D_SVT_TEXTURE1D:
        case D3D_SVT_TEXTURE2D:
        case D3D_SVT_TEXTURE3D:
        case D3D_SVT_TEXTURECUBE:
        case D3D_SVT_SAMPLER:
        case D3D_SVT_BUFFER:
        case D3D_SVT_CBUFFER:
        case D3D_SVT_TBUFFER:
        case D3D_SVT_TEXTURE1DARRAY:
        case D3D_SVT_TEXTURE2DARRAY:
        case D3D_SVT_TEXTURE2DMS:
        case D3D_SVT_TEXTURE2DMSARRAY:
        case D3D_SVT_TEXTURECUBEARRAY:
        case D3D_SVT_RWTEXTURE1D:
        case D3D_SVT_RWTEXTURE1DARRAY:
        case D3D_SVT_RWTEXTURE2D:
        case D3D_SVT_RWTEXTURE2DARRAY:
        case D3D_SVT_RWTEXTURE3D:
        case D3D_SVT_RWBUFFER:
        case D3D_SVT_BYTEADDRESS_BUFFER:
        case D3D_SVT_RWBYTEADDRESS_BUFFER:
        case D3D_SVT_STRUCTURED_BUFFER:
        case D3D_SVT_RWSTRUCTURED_BUFFER:
        case D3D_SVT_APPEND_STRUCTURED_BUFFER:
        case D3D_SVT_CONSUME_STRUCTURED_BUFFER:
          break;

        default:
          throw std::invalid_argument("Type (object) " + std::to_string(i) +
                                      " is of invalid type");
        }

        if (type.Rows || type.Columns)
          throw std::invalid_argument("Type (object) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

      break;

    default:
      throw std::invalid_argument("Type " + std::to_string(i) +
                                  " has an invalid class");
    }
  }
};

static HRESULT DoSimpleReWrite(DxcLangExtensionsHelper *pHelper,
                               LPCSTR pFileName, ASTUnit::RemappedFile *pRemap,
                               hlsl::options::DxcOpts &opts,
                               DxcDefine *pDefines, UINT32 defineCount,
                               std::string &warnings, std::string &result,
                               dxcutil::DxcArgsFileSystem *msfPtr) {
  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;

  GenerateAST(pHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
              opts, msfPtr, w);

  TranslationUnitDecl *tu = astHelper.tu;

  if (opts.RWOpt.ConsistentBindings || opts.RWOpt.ReflectHLSLBasics) {
    GenerateConsistentBindings(*tu, opts.AutoBindingSpace);
  }

  ReflectionMask reflectMask = ReflectionMask_None;

  if(opts.RWOpt.ReflectHLSLBasics)
    reflectMask |= ReflectionMask_Basics;

  if(opts.RWOpt.ReflectHLSLFunctions)
    reflectMask |= ReflectionMask_Functions;

  if(opts.RWOpt.ReflectHLSLNamespaces)
    reflectMask |= ReflectionMask_Namespaces;

  if(opts.RWOpt.ReflectHLSLUserTypes)
    reflectMask |= ReflectionMask_UserTypes;

  if(opts.RWOpt.ReflectHLSLFunctionInternals)
    reflectMask |= ReflectionMask_FunctionInternals;

  if(opts.RWOpt.ReflectHLSLVariables)
    reflectMask |= ReflectionMask_Variables;

  if (reflectMask) {

    DxcReflectionData Refl;
    ReflectHLSL(astHelper, Refl, opts.AutoBindingSpace, reflectMask,
                opts.DefaultRowMajor);

    RecursePrint(Refl, 0, 0, 0);

    std::vector<std::byte> bytes;
    Refl.Dump(bytes);

    DxcReflectionData Deserialized(bytes);

    assert(Deserialized == Refl && "Dump or Deserialize doesn't match");

    printf("Reflection size: %" PRIu64 "\n", bytes.size());
  }

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
  } else if (!opts.RWOpt.ConsistentBindings && (reflectMask & ReflectionMask_Basics)) {
    o << "// Rewrite unchanged result:\n";
  }

  ASTContext &C = tu->getASTContext();

  FunctionDecl *entryFnDecl = nullptr;
  if (opts.RWOpt.ExtractEntryUniforms) {
    DeclContext::lookup_result l =
        tu->lookup(DeclarationName(&C.Idents.get(opts.EntryPoint)));
    if (l.empty()) {
      w << "//entry point not found\n";
      return E_FAIL;
    }
    entryFnDecl = dyn_cast_or_null<FunctionDecl>(l.front());
    if (!HasUniformParams(entryFnDecl))
      entryFnDecl = nullptr;
  }

  PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());
  p.HLSLOmitDefaultTemplateParams = 1;
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
      : TheRewriter(R), SourceMgr(R.getSourceMgr()), tu(tu), helper(helper),
        bNeedLineInfo(false) {}

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

// Preprocess rewritten files.
HRESULT preprocessRewrittenFiles(DxcLangExtensionsHelper *pExtHelper,
                                 Rewriter &R, LPCSTR pFileName,
                                 ASTUnit::RemappedFile *pRemap,
                                 hlsl::options::DxcOpts &opts,
                                 DxcDefine *pDefines, UINT32 defineCount,
                                 raw_string_ostream &w, raw_string_ostream &o,
                                 dxcutil::DxcArgsFileSystem *msfPtr,
                                 IMalloc *pMalloc) {

  CComPtr<AbstractMemoryStream> pOutputStream;
  IFT(CreateMemoryStream(pMalloc, &pOutputStream));

  raw_stream_ostream outStream(pOutputStream.p);
  // TODO: how to reuse msfPtr when ReigsterOutputStream.
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
  SetupCompilerForPreprocess(compiler, pExtHelper, pFileName, diagPrinter.get(),
                             pPreprocessRemap.get(), opts, pDefines,
                             defineCount, msfPtr);

  auto &sourceManager = R.getSourceMgr();
  auto &preprocessorOpts = compiler.getPreprocessorOpts();
  // Map rewrite buf to source manager of preprocessor compiler.
  for (auto it = R.buffer_begin(); it != R.buffer_end(); it++) {
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
    msfPtr->UnRegisterOutputStream();
  } catch (Exception &exp) {
    w << exp.msg;
    return E_FAIL;
  } catch (...) {
    return E_FAIL;
  }
  return S_OK;
}

HRESULT DoReWriteWithLineDirective(
    DxcLangExtensionsHelper *pExtHelper, LPCSTR pFileName,
    ASTUnit::RemappedFile *pRemap, hlsl::options::DxcOpts &opts,
    DxcDefine *pDefines, UINT32 defineCount, std::string &warnings,
    std::string &result, dxcutil::DxcArgsFileSystem *msfPtr, IMalloc *pMalloc) {
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
  preprocessRewrittenFiles(pExtHelper, rewriter, pFileName, pRemap, opts,
                           pDefines, defineCount, w, o, msfPtr, pMalloc);

  WriteMacroDefines(astHelper.semanticMacros, o);
  if (opts.RWOpt.KeepUserMacro)
    WriteMacroDefines(astHelper.userMacros, o);

  // Flush and return results.
  o.flush();
  w.flush();

  return S_OK;
}

template <typename DT>
void printWithNamespace(DT *VD, raw_string_ostream &OS, PrintingPolicy &p) {
  SmallVector<StringRef, 2> namespaceList;
  auto const *Context = VD->getDeclContext();
  while (const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(Context)) {
    namespaceList.emplace_back(ND->getName());
    Context = ND->getDeclContext();
  }
  for (auto it = namespaceList.rbegin(); it != namespaceList.rend(); ++it) {
    OS << "namespace " << *it << " {\n";
  }

  VD->print(OS, p);
  OS << ";\n";
  for (unsigned i = 0; i < namespaceList.size(); ++i) {
    OS << "}\n";
  }
}

void printTypeWithoutMethodBody(const TypeDecl *TD, raw_string_ostream &OS,
                                PrintingPolicy &p) {
  PrintingPolicy declP(p);
  declP.HLSLOnlyDecl = true;
  printWithNamespace(TD, OS, declP);
}

class MethodsVisitor : public DeclVisitor<MethodsVisitor> {
public:
  MethodsVisitor(raw_string_ostream &o, PrintingPolicy &p) : OS(o), declP(p) {
    declP.HLSLNoinlineMethod = true;
  }

  void VisitFunctionDecl(FunctionDecl *f) {
    // Don't need to do namespace, the location is not change.
    f->print(OS, declP);
    return;
  }
  void VisitDeclContext(DeclContext *DC) {
    SmallVector<Decl *, 2> Decls;
    for (DeclContext::decl_iterator D = DC->decls_begin(),
                                    DEnd = DC->decls_end();
         D != DEnd; ++D) {

      // Don't print ObjCIvarDecls, as they are printed when visiting the
      // containing ObjCInterfaceDecl.
      if (isa<ObjCIvarDecl>(*D))
        continue;

      // Skip over implicit declarations in pretty-printing mode.
      if (D->isImplicit())
        continue;

      Visit(*D);
    }
  }
  void VisitCXXRecordDecl(CXXRecordDecl *D) {

    if (D->isCompleteDefinition()) {
      VisitDeclContext(D);
    }
  }

private:
  raw_string_ostream &OS;
  PrintingPolicy declP;
};

HRESULT DoRewriteGlobalCB(DxcLangExtensionsHelper *pExtHelper, LPCSTR pFileName,
                          ASTUnit::RemappedFile *pRemap,
                          hlsl::options::DxcOpts &opts, DxcDefine *pDefines,
                          UINT32 defineCount, std::string &warnings,
                          std::string &result,
                          dxcutil::DxcArgsFileSystem *msfPtr,
                          IMalloc *pMalloc) {
  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;
  GenerateAST(pExtHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
              opts, msfPtr, w);

  if (astHelper.bHasErrors)
    return E_FAIL;

  TranslationUnitDecl *tu = astHelper.tu;
  // Collect global constants.
  SmallVector<VarDecl *, 128> globalConstants;
  GlobalCBVisitor visitor(globalConstants);
  visitor.TraverseDecl(tu);

  // Collect types for global constants.
  MapVector<const TypeDecl *, DenseSet<const TypeDecl *>> typeDepMap;
  TypeVisitor tyVisitor(typeDepMap);

  for (VarDecl *VD : globalConstants) {
    QualType Type = VD->getType();
    tyVisitor.TraverseType(Type);
  }

  ASTContext &C = tu->getASTContext();
  Rewriter R(C.getSourceManager(), C.getLangOpts());

  std::string globalCBStr;
  raw_string_ostream OS(globalCBStr);

  PrintingPolicy p = PrintingPolicy(C.getPrintingPolicy());

  // Sort types with typeDepMap.
  SmallVector<const TypeDecl *, 32> sortedGlobalConstantTypes;
  while (!typeDepMap.empty()) {

    SmallSet<const TypeDecl *, 4> noDepTypes;

    for (auto it : typeDepMap) {
      const TypeDecl *TD = it.first;
      auto &dep = it.second;
      if (dep.empty()) {
        sortedGlobalConstantTypes.emplace_back(TD);
        noDepTypes.insert(TD);
      } else {
        for (auto *depDecl : dep) {
          if (typeDepMap.count(depDecl) == 0) {
            noDepTypes.insert(depDecl);
          }
        }

        for (auto *noDepDecl : noDepTypes) {
          if (dep.count(noDepDecl))
            dep.erase(noDepDecl);
        }
        if (dep.empty()) {
          sortedGlobalConstantTypes.emplace_back(TD);
          noDepTypes.insert(TD);
        }
      }
    }

    for (auto *noDepDecl : noDepTypes)
      typeDepMap.erase(noDepDecl);
  }

  // Move all type decl to top of tu.
  for (const TypeDecl *TD : sortedGlobalConstantTypes) {
    printTypeWithoutMethodBody(TD, OS, p);

    std::string methodsStr;
    raw_string_ostream methodsOS(methodsStr);
    MethodsVisitor Visitor(methodsOS, p);
    Visitor.Visit(const_cast<TypeDecl *>(TD));
    methodsOS.flush();
    R.ReplaceText(TD->getSourceRange(), methodsStr);
    // TODO: remove ; for type decl.
  }

  OS << "cbuffer GlobalCB {\n";
  // Create HLSLBufferDecl after the types.
  for (VarDecl *VD : globalConstants) {
    printWithNamespace(VD, OS, p);
    R.RemoveText(VD->getSourceRange());
    // TODO: remove ; for var decl.
  }
  OS << "}\n";

  OS.flush();

  // Cannot find begin of tu, just write first when output.
  // R.InsertTextBefore(tu->decls_begin()->getLocation(), globalCBStr);
  o << globalCBStr;

  // Preprocess rewritten files.
  preprocessRewrittenFiles(pExtHelper, R, pFileName, pRemap, opts, pDefines,
                           defineCount, w, o, msfPtr, pMalloc);

  WriteMacroDefines(astHelper.semanticMacros, o);
  if (opts.RWOpt.KeepUserMacro)
    WriteMacroDefines(astHelper.userMacros, o);

  // Flush and return results.
  o.flush();
  w.flush();

  return S_OK;
}

} // namespace

class DxcRewriter : public IDxcRewriter2, public IDxcLangExtensions3 {
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
                                 IDxcLangExtensions, IDxcLangExtensions2,
                                 IDxcLangExtensions3>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE RemoveUnusedGlobals(
      IDxcBlobEncoding *pSource, LPCWSTR pEntryPoint, DxcDefine *pDefines,
      UINT32 defineCount, IDxcOperationResult **ppResult) override {

    if (pSource == nullptr || ppResult == nullptr ||
        (defineCount > 0 && pDefines == nullptr))
      return E_INVALIDARG;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem *msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(),
                     utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
          llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(
          new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      CW2A utf8EntryPoint(pEntryPoint);

      std::string errors;
      std::string rewrite;
      LPCWSTR pOutputName = nullptr; // TODO: Fill this in
      HRESULT status = DoRewriteUnused(
          &m_langExtensionsHelper, fakeName, pRemap.get(), utf8EntryPoint,
          pDefines, defineCount, true /*removeGlobals*/,
          false /*removeFunctions*/, errors, rewrite, nullptr);
      return DxcResult::Create(
          status, DXC_OUT_HLSL,
          {DxcOutputObject::StringOutput(
               DXC_OUT_HLSL, CP_UTF8, // TODO: Support DefaultTextCodePage
               rewrite.c_str(), pOutputName),
           DxcOutputObject::ErrorOutput(
               CP_UTF8, // TODO Support DefaultTextCodePage
               errors.c_str())},
          ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE RewriteUnchanged(
      IDxcBlobEncoding *pSource, DxcDefine *pDefines, UINT32 defineCount,
      IDxcOperationResult **ppResult) override {
    if (pSource == nullptr || ppResult == nullptr ||
        (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    LPCSTR fakeName = "input.hlsl";

    try {
      ::llvm::sys::fs::MSFileSystem *msfPtr;
      IFT(CreateMSFileSystemForDisk(&msfPtr));
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      StringRef Data(utf8Source->GetStringPointer(),
                     utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
          llvm::MemoryBuffer::getMemBufferCopy(Data, fakeName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(
          new ASTUnit::RemappedFile(fakeName, pBuffer.release()));

      hlsl::options::DxcOpts opts;
      opts.HLSLVersion = hlsl::LangStd::v2015;

      std::string errors;
      std::string rewrite;
      HRESULT status =
          DoSimpleReWrite(&m_langExtensionsHelper, fakeName, pRemap.get(), opts,
                          pDefines, defineCount, errors, rewrite, nullptr);
      return DxcResult::Create(
          status, DXC_OUT_HLSL,
          {DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
                                         rewrite.c_str(), DxcOutNoName),
           DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                        errors.c_str())},
          ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE RewriteUnchangedWithInclude(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName, DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler, UINT32 rewriteOption,
      IDxcOperationResult **ppResult) override {
    if (pSource == nullptr || ppResult == nullptr ||
        (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    CW2A utf8SourceName(pSourceName);
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

      hlsl::options::DxcOpts opts;
      opts.HLSLVersion = hlsl::LangStd::v2015;

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
      return DxcResult::Create(
          status, DXC_OUT_HLSL,
          {DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
                                         rewrite.c_str(), DxcOutNoName),
           DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                        errors.c_str())},
          ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  HRESULT STDMETHODCALLTYPE RewriteWithOptions(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName,
      // Compiler arguments
      LPCWSTR *pArguments, UINT32 argCount,
      // Defines
      DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler,
      IDxcOperationResult **ppResult) override {

    if (pSource == nullptr || ppResult == nullptr ||
        (argCount > 0 && pArguments == nullptr) ||
        (defineCount > 0 && pDefines == nullptr))
      return E_POINTER;

    *ppResult = nullptr;

    DxcThreadMalloc TM(m_pMalloc);

    CComPtr<IDxcBlobUtf8> utf8Source;
    IFR(hlsl::DxcGetBlobAsUtf8(pSource, m_pMalloc, &utf8Source));

    CW2A utf8SourceName(pSourceName);
    LPCSTR fName = utf8SourceName.m_psz;

    try {
      dxcutil::DxcArgsFileSystem *msfPtr = dxcutil::CreateDxcArgsFileSystem(
          utf8Source, pSourceName, pIncludeHandler);
      std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
      ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
      IFTLLVM(pts.error_code());

      hlsl::options::MainArgs mainArgs(argCount, pArguments, 0);

      hlsl::options::DxcOpts opts;
      IFR(ReadOptsAndValidate(mainArgs, opts, ppResult));
      HRESULT hr;
      if (*ppResult && SUCCEEDED((*ppResult)->GetStatus(&hr)) && FAILED(hr)) {
        // Looks odd, but this call succeeded enough to allocate a result
        return S_OK;
      }

      StringRef Data(utf8Source->GetStringPointer(),
                     utf8Source->GetStringLength());
      std::unique_ptr<llvm::MemoryBuffer> pBuffer(
          llvm::MemoryBuffer::getMemBufferCopy(Data, fName));
      std::unique_ptr<ASTUnit::RemappedFile> pRemap(
          new ASTUnit::RemappedFile(fName, pBuffer.release()));

      if (opts.RWOpt.DeclGlobalCB) {
        std::string errors;
        std::string rewrite;
        HRESULT status = S_OK;
        status = DoRewriteGlobalCB(&m_langExtensionsHelper, fName, pRemap.get(),
                                   opts, pDefines, defineCount, errors, rewrite,
                                   msfPtr, m_pMalloc);
        if (status != S_OK) {
          return S_OK;
        }

        pBuffer = llvm::MemoryBuffer::getMemBufferCopy(rewrite, fName);
        pRemap.reset(new ASTUnit::RemappedFile(fName, pBuffer.release()));
      }
      std::string errors;
      std::string rewrite;
      HRESULT status = S_OK;
      if (opts.RWOpt.WithLineDirective) {
        status = DoReWriteWithLineDirective(
            &m_langExtensionsHelper, fName, pRemap.get(), opts, pDefines,
            defineCount, errors, rewrite, msfPtr, m_pMalloc);
      } else {
        status =
            DoSimpleReWrite(&m_langExtensionsHelper, fName, pRemap.get(), opts,
                            pDefines, defineCount, errors, rewrite, msfPtr);
      }
      return DxcResult::Create(
          status, DXC_OUT_HLSL,
          {DxcOutputObject::StringOutput(DXC_OUT_HLSL, opts.DefaultTextCodePage,
                                         rewrite.c_str(), DxcOutNoName),
           DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                        errors.c_str())},
          ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }
};

HRESULT CreateDxcRewriter(REFIID riid, LPVOID *ppv) {
  CComPtr<DxcRewriter> isense = DxcRewriter::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(isense.p);
  return isense.p->QueryInterface(riid, ppv);
}
