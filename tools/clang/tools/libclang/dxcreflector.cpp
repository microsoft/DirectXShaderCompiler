///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcreflector.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/HlslTypes.h"
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

#include "dxc/DxcReflection/DxcReflection.h"

using namespace llvm;
using namespace clang;
using namespace hlsl;

struct ASTHelper {
  CompilerInstance compiler;
  TranslationUnitDecl *tu;
  ParsedSemanticDefineList semanticMacros;
  ParsedSemanticDefineList userMacros;
  bool bHasErrors;
};

struct DxcHLSLReflection : public IDxcHLSLReflection {

  DxcHLSLReflectionData data{};
  std::vector<uint32_t> childCountsNonRecursive;
  std::unordered_map<uint32_t, std::vector<uint32_t>> childrenNonRecursive;

  DxcHLSLReflection() = default;

  void Finalize() {

    data.GenerateNameLookupTable();

    childCountsNonRecursive.resize(data.Nodes.size());
    childrenNonRecursive.clear();

    for (uint32_t i = 0; i < (uint32_t)data.Nodes.size(); ++i) {

      const DxcHLSLNode &node = data.Nodes[i];
      uint32_t childCount = 0;

      for (uint32_t j = 0; j < node.GetChildCount(); ++j, ++childCount) {
        childrenNonRecursive[i].push_back(i + 1 + j);
        j += data.Nodes[i + 1 + j].GetChildCount();
      }

      childCountsNonRecursive[i] = childCount;
    }
  }

  DxcHLSLReflection(DxcHLSLReflectionData &&moved) : data(moved) {
    Finalize();
  }

  DxcHLSLReflection &operator=(DxcHLSLReflection &&moved) {
    data = std::move(moved.data);
    Finalize();
    return *this;
  }

  //Conversion of DxcHLSL structs to D3D12_HLSL standardized structs

  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_HLSL_REFLECTION_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    *pDesc = {data.Features,
              0, //TODO: data.ConstantBuffers,
              uint32_t(data.Registers.size()),
              uint32_t(data.Functions.size()),
              uint32_t(data.Enums.size()),
              0, // TODO: Structs
              uint32_t(data.Nodes.size())};

    return S_OK;
  }

  STDMETHOD(GetResourceBindingDesc)
      (THIS_ _In_ UINT ResourceIndex,
          _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (ResourceIndex >= data.Registers.size())
      return E_INVALIDARG;

    const DxcHLSLRegister &reg = data.Registers[ResourceIndex];

    LPCSTR name =
        data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? data.Strings[data.NodeSymbols[reg.NodeId].NameId].c_str()
            : "";

    *pDesc = D3D12_SHADER_INPUT_BIND_DESC{
        name,
        (D3D_SHADER_INPUT_TYPE)reg.Type,
        uint32_t(-1),       //Invalid bindPoint, depending on backend we might want to change it
        reg.BindCount,

        reg.uFlags,
        (D3D_RESOURCE_RETURN_TYPE) reg.ReturnType,
        (D3D_SRV_DIMENSION) reg.Dimension,
        uint32_t(-1),       // Also no valid data depending on backend
        uint32_t(-1),       //Invalid space (see bindPoint ^)
        reg.NodeId
    };

    return S_OK;
  }
  
  STDMETHOD(GetEnumDesc)
      (THIS_ _In_ UINT EnumIndex, _Out_ D3D12_HLSL_ENUM_DESC* pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (EnumIndex >= data.Enums.size())
      return E_INVALIDARG;

    const DxcHLSLEnumDesc &enm = data.Enums[EnumIndex];

    LPCSTR name =
        data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? data.Strings[data.NodeSymbols[enm.NodeId].NameId].c_str()
            : "";
    
    *pDesc = D3D12_HLSL_ENUM_DESC{
        name, uint32_t(data.Nodes[enm.NodeId].GetChildCount()), enm.Type};

    return S_OK;
  }

  STDMETHOD(GetEnumValueByIndex)
      (THIS_ _In_ UINT EnumIndex, _In_ UINT ValueIndex,
          _Out_ D3D12_HLSL_ENUM_VALUE *pValueDesc) override {

    IFR(ZeroMemoryToOut(pValueDesc));

    if (EnumIndex >= data.Enums.size())
      return E_INVALIDARG;

    const DxcHLSLEnumDesc &enm = data.Enums[EnumIndex];
    const DxcHLSLNode &parent = data.Nodes[enm.NodeId];

    if (ValueIndex >= parent.GetChildCount())
      return E_INVALIDARG;

    LPCSTR name =
        data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? data.Strings[data.NodeSymbols[enm.NodeId].NameId].c_str()
            : "";

    const DxcHLSLNode &node = data.Nodes[enm.NodeId + 1 + ValueIndex];

    *pValueDesc =
        D3D12_HLSL_ENUM_VALUE{name, data.EnumValues[node.GetLocalId()].Value};

    return S_OK;
  }

  STDMETHOD(GetAnnotationByIndex)
  (THIS_ _In_ UINT NodeId, _In_ UINT Index,
   _Out_ D3D12_HLSL_ANNOTATION *pAnnotation) override {

    IFR(ZeroMemoryToOut(pAnnotation));

    if (NodeId >= data.Nodes.size())
      return E_INVALIDARG;

    const DxcHLSLNode &node = data.Nodes[NodeId];

    if (Index >= node.GetAnnotationCount())
      return E_INVALIDARG;

    const DxcHLSLAnnotation &annotation =
        data.Annotations[node.GetAnnotationStart() + Index];

    *pAnnotation = D3D12_HLSL_ANNOTATION{
        data.StringsNonDebug[annotation.GetStringNonDebug()].c_str(),
        annotation.GetIsBuiltin()};

    return S_OK;
  }

  STDMETHOD(GetFunctionDesc)
  (THIS_ _In_ UINT FunctionIndex,
   THIS_ _Out_ D3D12_HLSL_FUNCTION_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (FunctionIndex >= data.Functions.size())
      return E_INVALIDARG;

    const DxcHLSLFunction &func = data.Functions[FunctionIndex];

    LPCSTR name =
        data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? data.Strings[data.NodeSymbols[func.NodeId].NameId].c_str()
            : "";

    *pDesc = D3D12_HLSL_FUNCTION_DESC{name, func.GetNumParameters(),
                                      func.HasReturn()};

    return S_OK;
  }

  STDMETHOD(GetNodeDesc)
      (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (NodeId >= data.Nodes.size())
      return E_INVALIDARG;

    LPCSTR name =
        data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? data.Strings[data.NodeSymbols[NodeId].NameId].c_str()
                      : "";

    const DxcHLSLNode &node = data.Nodes[NodeId];

    *pDesc = D3D12_HLSL_NODE{name,
                             node.GetNodeType(),
                             node.GetLocalId(),
                             childCountsNonRecursive[NodeId],
                             node.GetParentId(),
                             node.GetAnnotationCount()};

    return S_OK;
  }

  STDMETHOD(GetChildNode)
      (THIS_ _In_ UINT NodeId, THIS_ _In_ UINT ChildId,
          _Out_ UINT *pChildNodeId) override {

    IFR(ZeroMemoryToOut(pChildNodeId));

    if (NodeId >= data.Nodes.size())
      return E_INVALIDARG;

    auto it = childrenNonRecursive.find(NodeId);

    if (it == childrenNonRecursive.end() || ChildId >= it->second.size())
      return E_INVALIDARG;

    *pChildNodeId = it->second[ChildId];
    return S_OK;
  }

  STDMETHOD(GetChildDesc)
      (THIS_ _In_ UINT NodeId, THIS_ _In_ UINT ChildId,
          _Out_ D3D12_HLSL_NODE *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    uint32_t childNodeId;
    IFR(GetChildNode(NodeId, ChildId, &childNodeId));

    return GetNodeDesc(childNodeId, pDesc);
  }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByIndex)
  (THIS_ _In_ UINT Index) PURE;

  // Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)
  (THIS_ _In_ UINT FunctionIndex, THIS_ _In_ INT ParameterIndex) PURE;

  STDMETHOD(GetStructTypeByIndex)
  (THIS_ _In_ UINT StructIndex,
   _Outptr_ ID3D12ShaderReflectionType **ppType) PURE;
  
  STDMETHOD(GetNodeSymbolDesc)
  (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE_SYMBOL *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (!(data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO))
      return E_UNEXPECTED;

    if (NodeId >= data.Nodes.size())
      return E_INVALIDARG;

    const DxcHLSLNodeSymbol &nodeSymbol = data.NodeSymbols[NodeId];

    *pDesc = D3D12_HLSL_NODE_SYMBOL{
        data.Strings[data.Sources[nodeSymbol.FileSourceId]].c_str(),
        nodeSymbol.GetSourceLineStart(), nodeSymbol.SourceLineCount,
        nodeSymbol.GetSourceColumnStart(), nodeSymbol.GetSourceColumnEnd()};

    return S_OK;
  }

  //Helper for conversion between symbol names

  // TODO: GetConstantBufferByIndex
  // TODO: GetConstantBufferByName
  // TODO: GetFunctionParameter
  // TODO: GetStructByIndex
  // TODO: GetStructTypeByName
  // TODO: GetStructTypeByIndex
  // TODO: types, arrays

  STDMETHOD(GetNodeByName)
      (THIS_ _In_ LPCSTR Name, _Out_ UINT *pNodeId) override {

    if (!pNodeId)
      return E_POINTER;

    *pNodeId = (UINT)-1;

    auto it = data.FullyResolvedToNodeId.find(Name);

    if (it == data.FullyResolvedToNodeId.end())
      return E_INVALIDARG;

    *pNodeId = it->second;
    return S_OK;
  }

  STDMETHOD(GetNodeDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_NODE *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    return GetNodeDesc(nodeId, pDesc);
  }

  STDMETHOD(GetNodeSymbolDescByName)
      (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_NODE_SYMBOL *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    return GetNodeSymbolDesc(nodeId, pDesc);
  }

  STDMETHOD(GetResourceBindingDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER)
      return E_INVALIDARG;

    return GetResourceBindingDesc(node.GetLocalId(), pDesc);
  }

  STDMETHOD(GetEnumDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_ENUM_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM)
      return E_INVALIDARG;

    return GetEnumDesc(node.GetLocalId(), pDesc);
  }

  STDMETHOD(GetEnumValueByNameAndIndex)
  (THIS_ _In_ LPCSTR Name, _In_ UINT ValueIndex,
   _Out_ D3D12_HLSL_ENUM_VALUE *pValueDesc) override {

    IFR(ZeroMemoryToOut(pValueDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM)
      return E_INVALIDARG;

    return GetEnumValueByIndex(node.GetLocalId(), ValueIndex, pValueDesc);
  }

  STDMETHOD(GetAnnotationByIndexAndName)
      (THIS_ _In_ LPCSTR Name, _In_ UINT Index,
          _Out_ D3D12_HLSL_ANNOTATION *pAnnotation) override {

    IFR(ZeroMemoryToOut(pAnnotation));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    return GetAnnotationByIndex(nodeId, Index, pAnnotation);
  }

  STDMETHOD(GetFunctionDescByName)
  (THIS_ _In_ LPCSTR Name,
   THIS_ _Out_ D3D12_HLSL_FUNCTION_DESC *pDesc) override{

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_FUNCTION)
      return E_INVALIDARG;

    return GetFunctionDesc(node.GetLocalId(), pDesc);
  }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByName)
  (THIS_ _In_ LPCSTR Name) PURE;

  STDMETHOD(GetStructTypeByName)
  (THIS_ _In_ LPCSTR Name, _Outptr_ ID3D12ShaderReflectionType **ppType) PURE;
};

namespace {

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
  compiler.getLangOpts().PreserveUnknownAnnotations =
      opts.RWOpt.ReflectHLSLBasics;
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

void SetupCompiler(CompilerInstance &compiler,
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

  SetupCompiler(
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
  return S_OK;
}

HRESULT GetFromSource(DxcLangExtensionsHelper *pHelper, LPCSTR pFileName,
                             ASTUnit::RemappedFile *pRemap,
                             hlsl::options::DxcOpts &opts, DxcDefine *pDefines,
                             UINT32 defineCount, std::string &warnings,
                             std::string &result,
                             dxcutil::DxcArgsFileSystem *msfPtr,
                             DxcHLSLReflectionData &reflection) {

  raw_string_ostream o(result);
  raw_string_ostream w(warnings);

  ASTHelper astHelper;

  HRESULT hr = GenerateAST(pHelper, pFileName, pRemap, pDefines, defineCount, astHelper,
              opts, msfPtr, w);

  if (FAILED(hr))
    return hr;

  if (astHelper.bHasErrors)
    return E_FAIL;

  TranslationUnitDecl *tu = astHelper.tu;

  D3D12_HLSL_REFLECTION_FEATURE reflectMask =
      D3D12_HLSL_REFLECTION_FEATURE_NONE;

  if (opts.RWOpt.ReflectHLSLBasics)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_BASICS;

  if (opts.RWOpt.ReflectHLSLFunctions)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS;

  if (opts.RWOpt.ReflectHLSLNamespaces)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES;

  if (opts.RWOpt.ReflectHLSLUserTypes)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES;

  if (opts.RWOpt.ReflectHLSLScopes)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_SCOPES;

  if (opts.RWOpt.ReflectHLSLVariables)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_VARIABLES;

  if (!opts.RWOpt.ReflectHLSLDisableSymbols)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!reflectMask)
    reflectMask = D3D12_HLSL_REFLECTION_FEATURE_ALL; 

  DxcHLSLReflectionData refl(astHelper.compiler, *astHelper.tu,
                              opts.AutoBindingSpace, reflectMask,
                              opts.DefaultRowMajor);

  //TODO: Debug

  refl.Printf();

  // Test serialization

  std::vector<std::byte> bytes;
  refl.Dump(bytes);

  DxcHLSLReflectionData deserialized(bytes, true);

  assert(deserialized == refl && "Dump or Deserialize doesn't match");

  printf("Reflection size: %" PRIu64 "\n", bytes.size());

  // Test stripping symbols

  refl.StripSymbols();
  refl.Printf();

  refl.Dump(bytes);

  DxcHLSLReflectionData deserialized2 = DxcHLSLReflectionData(bytes, false);

  assert(deserialized2 == refl && "Dump or Deserialize doesn't match");

  printf("Stripped reflection size: %" PRIu64 "\n", bytes.size());

  reflection = std::move(deserialized);

  // Flush and return results.
  o.flush();
  w.flush();

  return S_OK;
}

HRESULT ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
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
} // namespace

class DxcReflector : public IDxcHLSLReflector, public IDxcLangExtensions3 {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  DxcLangExtensionsHelper m_langExtensionsHelper;

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcReflector)
  DXC_LANGEXTENSIONS_HELPER_IMPL(m_langExtensionsHelper)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcHLSLReflector, IDxcLangExtensions,
                                 IDxcLangExtensions2, IDxcLangExtensions3>(
        this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE FromSource(
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

      DxcHLSLReflectionData reflection;

      std::string errors;
      std::string rewrite;
      HRESULT status = GetFromSource(&m_langExtensionsHelper, fName,
                                     pRemap.get(), opts, pDefines, defineCount,
                                     errors, rewrite, msfPtr, reflection);

      std::vector<std::byte> Bytes;

      if (SUCCEEDED(status))
        reflection.Dump(Bytes);

      return DxcResult::Create(
          status, DXC_OUT_OBJECT,
          {DxcOutputObject::ObjectOutput(Bytes.data(), Bytes.size()),
           DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                        errors.c_str())},
          ppResult);
    }
    CATCH_CPP_RETURN_HRESULT();
  }

  //TODO:

  HRESULT STDMETHODCALLTYPE FromBlob(IDxcBlob *data, IDxcHLSLReflection **ppReflection) override {
      
    if (!data || !data->GetBufferSize() || !ppReflection)
      return E_POINTER;

    /* TODO: */

    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE ToBlob(IDxcHLSLReflection *reflection,
      IDxcBlob **ppResult) override {

    if (!reflection || !ppResult)
      return E_POINTER;

    /*TODO:
    DxcHLSLReflection *refl = ...;

    std::vector<std::byte> bytes;
    refl->data.Dump(bytes);

    library->CreateBlobWithEncodingOnHeapCopy(
        vec.data(),                      // pointer to your data
        static_cast<UINT32>(vec.size()), // size in bytes
        CP_UTF8,                         // or 0 for "unknown"
        &blob);
        */

    return E_FAIL;
  }
};

HRESULT CreateDxcReflector(REFIID riid, LPVOID *ppv) {
  CComPtr<DxcReflector> isense = DxcReflector::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(isense.p);
  return isense.p->QueryInterface(riid, ppv);
}