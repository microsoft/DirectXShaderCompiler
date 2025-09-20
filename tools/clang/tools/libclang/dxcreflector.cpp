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

// We can't return nullptr instead, since that doesn't match old behavior...

class CHLSLInvalidSRType final : public ID3D12ShaderReflectionType {

  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc) override { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetMemberTypeByIndex)
  (UINT Index) override;

  STDMETHOD_(ID3D12ShaderReflectionType *, GetMemberTypeByName)
  (LPCSTR Name) override;

  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index) override {
    return "$Invalid";
  }

  STDMETHOD(IsEqual)(ID3D12ShaderReflectionType *pType) override {
    return E_FAIL;
  }
  STDMETHOD_(ID3D12ShaderReflectionType *, GetSubType)() override;
  STDMETHOD_(ID3D12ShaderReflectionType *, GetBaseClass)() override;
  STDMETHOD_(UINT, GetNumInterfaces)() override { return 0; }
  STDMETHOD_(ID3D12ShaderReflectionType *, GetInterfaceByIndex)
  (UINT uIndex) override;

  STDMETHOD(IsOfType)(ID3D12ShaderReflectionType *pType) override {
    return E_FAIL;
  }

  STDMETHOD(ImplementsInterface)(ID3D12ShaderReflectionType *pBase) override {
    return E_FAIL;
  }
};
static CHLSLInvalidSRType g_InvalidSRType;

ID3D12ShaderReflectionType *CHLSLInvalidSRType::GetMemberTypeByIndex(UINT) {
  return &g_InvalidSRType;
}
ID3D12ShaderReflectionType *CHLSLInvalidSRType::GetMemberTypeByName(LPCSTR) {
  return &g_InvalidSRType;
}
ID3D12ShaderReflectionType *CHLSLInvalidSRType::GetSubType() {
  return &g_InvalidSRType;
}
ID3D12ShaderReflectionType *CHLSLInvalidSRType::GetBaseClass() {
  return &g_InvalidSRType;
}
ID3D12ShaderReflectionType *CHLSLInvalidSRType::GetInterfaceByIndex(UINT) {
  return &g_InvalidSRType;
}

class CHLSLInvalidSRVariable final : public ID3D12ShaderReflectionVariable {

  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc) override {
    return E_FAIL;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetType)() override {
    return &g_InvalidSRType;
  }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetBuffer)() override;

  STDMETHOD_(UINT, GetInterfaceSlot)(UINT uIndex) override { return UINT_MAX; }
};
static CHLSLInvalidSRVariable g_InvalidSRVariable;

class CHLSLInvalidSRConstantBuffer final
    : public ID3D12ShaderReflectionConstantBuffer {

  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc) override {
    return E_FAIL;
  }

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByIndex)
  (UINT Index) override {
    return &g_InvalidSRVariable;
  }

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByName)
  (LPCSTR Name) override { return &g_InvalidSRVariable; }
};
static CHLSLInvalidSRConstantBuffer g_InvalidSRConstantBuffer;

ID3D12ShaderReflectionConstantBuffer *CHLSLInvalidSRVariable::GetBuffer() {
  return &g_InvalidSRConstantBuffer;
}

class CHLSLReflectionConstantBuffer;

class CHLSLReflectionType final : public ID3D12ShaderReflectionType1 {
  friend class CHLSLReflectionConstantBuffer;

protected:

  std::string m_Name;
  std::vector<std::string> m_MemberNames;
  std::unordered_map<std::string, uint32_t> m_NameToMemberId;
  std::vector<CHLSLReflectionType *> m_MemberTypes;
  CHLSLReflectionType *m_pBaseClass;

  const DxcHLSLReflectionData *m_Data;
  uint32_t m_TypeId;
  uint32_t m_Elements;
  D3D12_ARRAY_DESC m_ArrayDesc;

public:

  STDMETHOD(IsEqual)(ID3D12ShaderReflectionType *pType) override {
    // TODO: implement this check, if users actually depend on it
    return S_FALSE;
  }

  STDMETHOD(IsOfType)(ID3D12ShaderReflectionType *pType) override {
    // TODO: implement `class`-related features, if requested
    return S_FALSE;
  }

  STDMETHOD(GetArrayDesc)(THIS_ _Out_ D3D12_ARRAY_DESC *pArrayDesc) override {

    if (!pArrayDesc)
      return E_POINTER;

    *pArrayDesc = m_ArrayDesc;
    return S_OK;
  }

  HRESULT Initialize(
      const DxcHLSLReflectionData &Data, uint32_t TypeId,
      std::vector<CHLSLReflectionType> &Types /* Only access < TypeId*/) {

    m_TypeId = TypeId;
    m_Elements = 0;
    m_Data = &Data;

    ZeroMemoryToOut(&m_ArrayDesc);

    const DxcHLSLType &type = Data.Types[TypeId];

    if (type.IsArray()) {

      m_Elements = type.Is1DArray() ? type.Get1DElements() : 1;

      if (type.IsMultiDimensionalArray()) {

        const DxcHLSLArray &arr =
            Data.Arrays[type.GetMultiDimensionalArrayId()];

        m_ArrayDesc.ArrayDims = arr.ArrayElem();

        for (uint32_t i = 0; i < arr.ArrayElem(); ++i) {
          uint32_t len = Data.ArraySizes[arr.ArrayStart() + i];
          m_Elements *= len;
          m_ArrayDesc.ArrayLengths[i] = len;
        }
      }

      else {
        m_ArrayDesc.ArrayDims = 1;
        m_ArrayDesc.ArrayLengths[0] = type.Get1DElements();
      }
    }

    bool hasNames = Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

    if (hasNames)
      m_Name = Data.TypeNameIds[TypeId];

    uint32_t memberCount = type.GetMemberCount();

    m_MemberNames.resize(memberCount);
    m_MemberTypes.resize(memberCount);
    m_NameToMemberId.clear();

    for (uint32_t i = 0; i < memberCount; ++i) {
    
      uint32_t memberId = type.GetMemberStart() + i;

      m_MemberTypes[i] = &Types[Data.MemberTypeIds[memberId]];

      if (hasNames) {
      
        const std::string &name = Data.Strings[Data.MemberNameIds[memberId]];

        m_MemberNames[i] = name;
        m_NameToMemberId[name] = i;
      }
    }

    if (type.BaseClass != uint32_t(-1))
      m_pBaseClass = &Types[type.BaseClass];
  }

  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    const DxcHLSLType &type = m_Data->Types[m_TypeId];

    *pDesc = D3D12_SHADER_TYPE_DESC {

      (D3D_SHADER_VARIABLE_CLASS) type.Class,
      (D3D_SHADER_VARIABLE_TYPE) type.Type,
      type.Rows,
      type.Columns,

      m_Elements,
      uint32_t(m_MemberTypes.size()),
      0,                //TODO: Offset if we have one
      m_Name.c_str()
    };

    return S_OK;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetMemberTypeByIndex)
  (UINT Index) override {

    if (Index >= m_MemberTypes.size())
      return &g_InvalidSRType;

    return m_MemberTypes[Index];
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetMemberTypeByName)
  (LPCSTR Name) override {

    if (!Name)
      return &g_InvalidSRType;

    auto it = m_NameToMemberId.find(Name);
    return it == m_NameToMemberId.end()
               ? (ID3D12ShaderReflectionType *)&g_InvalidSRType
               : m_MemberTypes[it->second];
  }

  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index) override {

    if (Index >= m_MemberTypes.size())
      return nullptr;

    return m_MemberNames[Index].c_str();
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetSubType)() override {
    return nullptr;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetBaseClass)() override {
    return m_pBaseClass;
  }

  STDMETHOD_(UINT, GetNumInterfaces)() override {
    // HLSL interfaces have been deprecated
    return 0;
  }

  STDMETHOD(ImplementsInterface)(ID3D12ShaderReflectionType *pBase) override {
    // HLSL interfaces have been deprecated
    return S_FALSE;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetInterfaceByIndex)
  (UINT uIndex) override {
    // HLSL interfaces have been deprecated
    return nullptr;
  }
};

class CHLSLReflectionVariable final : public ID3D12ShaderReflectionVariable {
protected:
  CHLSLReflectionType *m_pType;
  CHLSLReflectionConstantBuffer *m_pBuffer;
  std::string m_Name;

public:
  void Initialize(CHLSLReflectionConstantBuffer *pBuffer,
                  CHLSLReflectionType *pType, std::string &&Name) {
    m_pBuffer = pBuffer;
    m_pType = pType;
    m_Name = Name;
  }

  LPCSTR GetName() const { return m_Name.c_str(); }

  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    *pDesc = D3D12_SHADER_VARIABLE_DESC{
        GetName(),
        0, // TODO: offset and size next time
        0,         D3D_SVF_USED, NULL, uint32_t(-1), 0, uint32_t(-1), 0};

    return S_OK;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetType)
  () override { return m_pType; }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetBuffer)() override;

  STDMETHOD_(UINT, GetInterfaceSlot)(UINT uArrayIndex) override {
    return UINT_MAX;
  }
};

class CHLSLReflectionConstantBuffer final
    : public ID3D12ShaderReflectionConstantBuffer {
protected:
  const DxcHLSLReflectionData *m_Data;
  uint32_t m_ChildCount;
  D3D_CBUFFER_TYPE m_BufferType;
  std::vector<CHLSLReflectionVariable> m_Variables;
  std::unordered_map<std::string, std::uint32_t> m_VariablesByName;

  // For StructuredBuffer arrays, Name will have [0] appended for each dimension
  // to match fxc behavior.
  std::string m_ReflectionName;

public:
  CHLSLReflectionConstantBuffer() = default;
  CHLSLReflectionConstantBuffer(CHLSLReflectionConstantBuffer &&other) {
    m_BufferType = other.m_BufferType;
    m_Data = other.m_Data;
    m_ChildCount = other.m_ChildCount;
    std::swap(m_ReflectionName, other.m_ReflectionName);
    std::swap(m_Variables, other.m_Variables);
    std::swap(m_VariablesByName, other.m_VariablesByName);
  }

  void Initialize(const DxcHLSLReflectionData &Data, uint32_t NodeId,
                  const std::unordered_map<uint32_t, std::vector<uint32_t>>
                      &ChildrenNonRecursive,
                  CHLSLReflectionConstantBuffer *ConstantBuffer,
                  std::vector<CHLSLReflectionType> &Types) {

    if (NodeId >= Data.Nodes.size())
      return;

    const DxcHLSLNode &node = Data.Nodes[NodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER)
      return;

    const std::vector<uint32_t> &children = ChildrenNonRecursive.at(NodeId);

    const DxcHLSLRegister &reg = Data.Registers[node.GetLocalId()];

    if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) {

      m_ReflectionName = Data.Strings[Data.NodeSymbols[NodeId].NameId];

      bool isCBuffer = reg.Type == D3D_SIT_CBUFFER;

      if (m_ReflectionName.size() && !isCBuffer) {

        uint32_t arrayDims = reg.ArrayId != uint32_t(-1)
                                 ? Data.Arrays[reg.ArrayId].ArrayElem()
                                 : (reg.BindCount > 1 ? 1 : 0);

        for (unsigned i = 0; i < arrayDims; ++i)
          m_ReflectionName += "[0]";
      }
    }

    else
      m_ReflectionName.clear();

    m_Data = &Data;
    m_ChildCount = uint32_t(children.size());
    m_BufferType = m_Data->Buffers[reg.BufferId].Type;

    m_VariablesByName.clear();
    m_Variables.resize(children.size());

    for (uint32_t i = 0, j = 0; i < m_ChildCount; ++i) {

      uint32_t childId = children[i];

      std::string name;

      if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
        name = Data.Strings[Data.NodeSymbols[childId].NameId];

      uint32_t typeId = Data.Nodes[childId].GetLocalId();

      m_Variables[i].Initialize(ConstantBuffer, &Types[typeId],
                                std::move(name));

      if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
        m_VariablesByName[m_Variables[i].GetName()] = i;
    }
  }

  //$Globals (only if the global scope contains any VARIABLE node)
  void InitializeGlobals(const DxcHLSLReflectionData &Data,
                  const std::vector<uint32_t> &Globals,
                  CHLSLReflectionConstantBuffer *ConstantBuffer,
                  std::vector<CHLSLReflectionType> &Types) {

    m_ReflectionName = "$Globals";

    m_Data = &Data;
    m_ChildCount = uint32_t(Globals.size());
    m_BufferType = D3D_CT_CBUFFER;

    m_VariablesByName.clear();
    m_Variables.resize(Globals.size());

    for (uint32_t i = 0, j = 0; i < m_ChildCount; ++i) {

      uint32_t childId = Globals[i];

      const DxcHLSLNode &node = Data.Nodes[childId];

      std::string name;

      if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
        name = Data.Strings[Data.NodeSymbols[childId].NameId];

      uint32_t typeId = Data.Nodes[childId].GetLocalId();

      m_Variables[i].Initialize(ConstantBuffer, &Types[typeId],
                                std::move(name));

      if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
        m_VariablesByName[m_Variables[i].GetName()] = i;
    }
  }

  LPCSTR GetName() const { return m_ReflectionName.c_str(); }

  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    *pDesc = D3D12_SHADER_BUFFER_DESC{
        GetName(), m_BufferType, m_ChildCount,
        0 // TODO: Size when we have it
    };

    return S_OK;
  }

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByIndex)
  (UINT Index) override {

    if (Index >= m_Variables.size())
      return &g_InvalidSRVariable;

    return &m_Variables[Index];
  }

  STDMETHOD_(ID3D12ShaderReflectionVariable *, GetVariableByName)
  (LPCSTR Name) override {

    if (NULL == Name)
      return &g_InvalidSRVariable;

    auto it = m_VariablesByName.find(Name);

    if (it == m_VariablesByName.end())
      return &g_InvalidSRVariable;

    return &m_Variables[it->second];
  }
};

ID3D12ShaderReflectionConstantBuffer *CHLSLReflectionVariable::GetBuffer() {
  return m_pBuffer;
}

struct DxcHLSLReflection : public IDxcHLSLReflection {

  DxcHLSLReflectionData Data{};

  std::vector<uint32_t> ChildCountsNonRecursive;
  std::unordered_map<uint32_t, std::vector<uint32_t>> ChildrenNonRecursive;

  std::vector<CHLSLReflectionConstantBuffer> ConstantBuffers;
  std::unordered_map<std::string, std::uint32_t> NameToConstantBuffers;

  std::vector<CHLSLReflectionType> Types;

  enum class FwdDeclType {
      STRUCT,
      UNION,
      ENUM,
      FUNCTION,
      COUNT
  };

  std::vector<uint32_t> NonFwdIds[int(FwdDeclType::COUNT)];

  std::unordered_map<std::string, uint32_t>
      NameToNonFwdIds[int(FwdDeclType::COUNT)];

  DxcHLSLReflection() = default;

  void Finalize() {

    Data.GenerateNameLookupTable();

    ChildCountsNonRecursive.resize(Data.Nodes.size());
    ChildrenNonRecursive.clear();

    bool hasSymbols = Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;
    std::vector<uint32_t> globalVars;

    for (uint32_t i = 0; i < uint32_t(Data.Nodes.size()); ++i) {

      const DxcHLSLNode &node = Data.Nodes[i];

      if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_VARIABLE &&
          !node.GetParentId())
        globalVars.push_back(i);

      // Filter out fwd declarations for structs, unions, functions, enums

      if (!node.IsFwdDeclare()) {

        FwdDeclType type = FwdDeclType::COUNT;

        switch (node.GetNodeType()) {

        case D3D12_HLSL_NODE_TYPE_STRUCT:
          type = FwdDeclType::STRUCT;
          break;

        case D3D12_HLSL_NODE_TYPE_UNION:
          type = FwdDeclType::UNION;
          break;

        case D3D12_HLSL_NODE_TYPE_FUNCTION:
          type = FwdDeclType::FUNCTION;
          break;

        case D3D12_HLSL_NODE_TYPE_ENUM:
          type = FwdDeclType::ENUM;
          break;
        }

        if (type != FwdDeclType::COUNT) {

          uint32_t typeId = node.GetLocalId();

          NonFwdIds[int(type)].push_back(typeId);

          if (hasSymbols)
            NameToNonFwdIds[int(type)]
                           [Data.Strings[Data.NodeSymbols[i].NameId]] = typeId;

          break;
        }
      }

      for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

        const DxcHLSLNode &nodej = Data.Nodes[i + 1 + j];

        // Filter out definitions that were previously fwd declared
        // And resolve fwd declarations

        if (nodej.IsFwdDeclare() && nodej.IsFwdBckDefined())
          ChildrenNonRecursive[i].push_back(nodej.GetFwdBck());

        if (!nodej.IsFwdBckDefined())
          ChildrenNonRecursive[i].push_back(i + 1 + j);

        j += nodej.GetChildCount();
      }

      ChildCountsNonRecursive[i] = uint32_t(ChildrenNonRecursive[i].size());
    }

    NameToConstantBuffers.clear();
    ConstantBuffers.resize(Data.Buffers.size() + !globalVars.empty());
    Types.resize(Data.Types.size());

    for (uint32_t i = 0; i < (uint32_t)Data.Types.size(); ++i)
      Types[i].Initialize(Data, i, Types);

    for (uint32_t i = 0; i < (uint32_t)Data.Buffers.size(); ++i) {

      ConstantBuffers[i].Initialize(Data, Data.Buffers[i].NodeId,
                                    ChildrenNonRecursive, &ConstantBuffers[i],
                                    Types);

      if (hasSymbols)
        NameToConstantBuffers[ConstantBuffers[i].GetName()] = i;
    }

    if (globalVars.size())
      ConstantBuffers[Data.Buffers.size()].InitializeGlobals(
          Data, globalVars, &ConstantBuffers[Data.Buffers.size()], Types);
  }

  DxcHLSLReflection(DxcHLSLReflectionData &&moved) : Data(moved) {
    Finalize();
  }

  DxcHLSLReflection &operator=(DxcHLSLReflection &&moved) {

    Data = std::move(moved.Data);
    ChildCountsNonRecursive = std::move(moved.ChildCountsNonRecursive);
    ChildrenNonRecursive = std::move(moved.ChildrenNonRecursive);
    ConstantBuffers = std::move(moved.ConstantBuffers);
    NameToConstantBuffers = std::move(moved.NameToConstantBuffers);
    Types = std::move(moved.Types);

    for (int i = 0; i < int(FwdDeclType::COUNT); ++i) {
      NonFwdIds[i] = std::move(moved.NonFwdIds[i]);
      NameToNonFwdIds[i] = std::move(moved.NameToNonFwdIds[i]);
    }

    return *this;
  }

  //Conversion of DxcHLSL structs to D3D12_HLSL standardized structs

  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_HLSL_REFLECTION_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    *pDesc = {Data.Features,
              uint32_t(Data.Buffers.size()),
              uint32_t(Data.Registers.size()),
              uint32_t(NonFwdIds[int(FwdDeclType::FUNCTION)].size()),
              uint32_t(NonFwdIds[int(FwdDeclType::ENUM)].size()),
              uint32_t(Data.Nodes.size()),
              uint32_t(Data.Types.size()),
              uint32_t(NonFwdIds[int(FwdDeclType::STRUCT)].size()),
              uint32_t(NonFwdIds[int(FwdDeclType::UNION)].size())};

    return S_OK;
  }

  STDMETHOD(GetResourceBindingDesc)
  (THIS_ _In_ UINT ResourceIndex,
   _Out_ D3D12_SHADER_INPUT_BIND_DESC1 *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (ResourceIndex >= Data.Registers.size())
      return E_INVALIDARG;

    const DxcHLSLRegister &reg = Data.Registers[ResourceIndex];

    LPCSTR name =
        Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? Data.Strings[Data.NodeSymbols[reg.NodeId].NameId].c_str()
            : "";

    if (reg.BindCount > 1) {

      if (reg.ArrayId != uint32_t(-1)) {

        const DxcHLSLArray &arr = Data.Arrays[reg.ArrayId];

        pDesc->ArrayInfo.ArrayDims = arr.ArrayElem();

        for (uint32_t i = 0; i < pDesc->ArrayInfo.ArrayDims; ++i)
          pDesc->ArrayInfo.ArrayLengths[i] =
              Data.ArraySizes[arr.ArrayStart() + i];
      }

      else {
        pDesc->ArrayInfo.ArrayDims = 1;
        pDesc->ArrayInfo.ArrayLengths[0] = reg.BindCount;
      }
    }

    pDesc->Desc = D3D12_SHADER_INPUT_BIND_DESC{
        name, D3D_SHADER_INPUT_TYPE(reg.Type),
        uint32_t(-1), // Invalid bindPoint, depending on backend we might
                      // want to change it
        reg.BindCount,

        reg.uFlags, D3D_RESOURCE_RETURN_TYPE(reg.ReturnType),
        D3D_SRV_DIMENSION(reg.Dimension),
        uint32_t(-1), // Also no valid data depending on backend
        uint32_t(-1), // Invalid space (see bindPoint ^)
        reg.NodeId};

    return S_OK;
  }
  
  STDMETHOD(GetEnumDesc)
      (THIS_ _In_ UINT EnumIndex, _Out_ D3D12_HLSL_ENUM_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (EnumIndex >= NonFwdIds[int(FwdDeclType::ENUM)].size())
      return E_INVALIDARG;

    const DxcHLSLEnumDesc &enm =
        Data.Enums[NonFwdIds[int(FwdDeclType::ENUM)][EnumIndex]];

    LPCSTR name =
        Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? Data.Strings[Data.NodeSymbols[enm.NodeId].NameId].c_str()
            : "";
    
    *pDesc = D3D12_HLSL_ENUM_DESC{
        name, uint32_t(Data.Nodes[enm.NodeId].GetChildCount()), enm.Type};

    return S_OK;
  }

  STDMETHOD(GetEnumValueByIndex)
      (THIS_ _In_ UINT EnumIndex, _In_ UINT ValueIndex,
          _Out_ D3D12_HLSL_ENUM_VALUE *pValueDesc) override {

    IFR(ZeroMemoryToOut(pValueDesc));

    if (EnumIndex >= NonFwdIds[int(FwdDeclType::ENUM)].size())
      return E_INVALIDARG;

    const DxcHLSLEnumDesc &enm = Data.Enums[NonFwdIds[int(FwdDeclType::ENUM)][EnumIndex]];
    const DxcHLSLNode &parent = Data.Nodes[enm.NodeId];

    if (ValueIndex >= parent.GetChildCount())
      return E_INVALIDARG;

    LPCSTR name =
        Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? Data.Strings[Data.NodeSymbols[enm.NodeId].NameId].c_str()
            : "";

    const DxcHLSLNode &node = Data.Nodes[enm.NodeId + 1 + ValueIndex];

    *pValueDesc =
        D3D12_HLSL_ENUM_VALUE{name, Data.EnumValues[node.GetLocalId()].Value};

    return S_OK;
  }

  STDMETHOD(GetAnnotationByIndex)
  (THIS_ _In_ UINT NodeId, _In_ UINT Index,
   _Out_ D3D12_HLSL_ANNOTATION *pAnnotation) override {

    IFR(ZeroMemoryToOut(pAnnotation));

    if (NodeId >= Data.Nodes.size())
      return E_INVALIDARG;

    const DxcHLSLNode &node = Data.Nodes[NodeId];

    if (Index >= node.GetAnnotationCount())
      return E_INVALIDARG;

    const DxcHLSLAnnotation &annotation =
        Data.Annotations[node.GetAnnotationStart() + Index];

    *pAnnotation = D3D12_HLSL_ANNOTATION{
        Data.StringsNonDebug[annotation.GetStringNonDebug()].c_str(),
        annotation.GetIsBuiltin()};

    return S_OK;
  }

  STDMETHOD(GetFunctionDesc)
  (THIS_ _In_ UINT FunctionIndex,
   THIS_ _Out_ D3D12_HLSL_FUNCTION_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (FunctionIndex >= NonFwdIds[int(FwdDeclType::FUNCTION)].size())
      return E_INVALIDARG;

    const DxcHLSLFunction &func =
        Data.Functions[NonFwdIds[int(FwdDeclType::FUNCTION)][FunctionIndex]];

    LPCSTR name =
        Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? Data.Strings[Data.NodeSymbols[func.NodeId].NameId].c_str()
            : "";

    *pDesc = D3D12_HLSL_FUNCTION_DESC{name, func.GetNumParameters(),
                                      func.HasReturn()};

    return S_OK;
  }

  STDMETHOD(GetNodeDesc)
      (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (NodeId >= Data.Nodes.size())
      return E_INVALIDARG;

    LPCSTR name = Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
                      ? Data.Strings[Data.NodeSymbols[NodeId].NameId].c_str()
                      : "";

    const DxcHLSLNode &node = Data.Nodes[NodeId];

    uint32_t localId = node.GetLocalId();
    uint32_t parentId = node.GetParentId();

    //Real local id is at definition

    if (node.IsFwdDeclare()) {
      if (node.IsFwdBckDefined())
        localId = Data.Nodes[node.GetFwdBck()].GetLocalId();
    }

    //Real parent is at declaration

    else if(node.IsFwdBckDefined())
      parentId = Data.Nodes[node.GetFwdBck()].GetParentId();

    *pDesc = D3D12_HLSL_NODE{name,
                             node.GetNodeType(),
                             localId,
                             ChildCountsNonRecursive[NodeId],
                             parentId,
                             node.GetAnnotationCount(),
                             node.IsFwdBckDefined() ? node.GetFwdBck()
                                                    : uint32_t(-1),
                             node.IsFwdDeclare()};

    return S_OK;
  }

  STDMETHOD(GetChildNode)
      (THIS_ _In_ UINT NodeId, THIS_ _In_ UINT ChildId,
          _Out_ UINT *pChildNodeId) override {

    IFR(ZeroMemoryToOut(pChildNodeId));

    if (NodeId >= Data.Nodes.size())
      return E_INVALIDARG;

    auto it = ChildrenNonRecursive.find(NodeId);

    if (it == ChildrenNonRecursive.end() || ChildId >= it->second.size())
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
  (THIS_ _In_ UINT Index) override {

    if (Index >= ConstantBuffers.size())
      return &g_InvalidSRConstantBuffer;

    return &ConstantBuffers[Index];
  }

  STDMETHOD(GetTypeByIndex)
  (THIS_ _In_ UINT Index,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (Index >= Types.size())
      return E_INVALIDARG;

    *ppType = &Types[Index];
    return S_OK;
  }

  //TODO:
  //// Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  //STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)
  //(THIS_ _In_ UINT FunctionIndex, THIS_ _In_ INT ParameterIndex) PURE;

  STDMETHOD(GetStructTypeByIndex)
  (THIS_ _In_ UINT Index,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (Index >= NonFwdIds[int(FwdDeclType::STRUCT)].size())
      return E_INVALIDARG;

    *ppType = &Types[NonFwdIds[int(FwdDeclType::STRUCT)][Index]];
    return S_OK;
  }

  STDMETHOD(GetUnionTypeByIndex)
  (THIS_ _In_ UINT Index,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (Index >= NonFwdIds[int(FwdDeclType::UNION)].size())
      return E_INVALIDARG;

    *ppType = &Types[NonFwdIds[int(FwdDeclType::UNION)][Index]];
    return S_OK;
  }
  
  STDMETHOD(GetNodeSymbolDesc)
  (THIS_ _In_ UINT NodeId, _Out_ D3D12_HLSL_NODE_SYMBOL *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    if (!(Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO))
      return E_UNEXPECTED;

    if (NodeId >= Data.Nodes.size())
      return E_INVALIDARG;

    const DxcHLSLNodeSymbol &nodeSymbol = Data.NodeSymbols[NodeId];

    *pDesc = D3D12_HLSL_NODE_SYMBOL{
        Data.Strings[Data.Sources[nodeSymbol.FileSourceId]].c_str(),
        nodeSymbol.GetSourceLineStart(), nodeSymbol.SourceLineCount,
        nodeSymbol.GetSourceColumnStart(), nodeSymbol.GetSourceColumnEnd()};

    return S_OK;
  }

  //Helper for conversion between symbol names

  // TODO: GetFunctionParameter
  // TODO: types, arrays

  STDMETHOD(GetNodeByName)
      (THIS_ _In_ LPCSTR Name, _Out_ UINT *pNodeId) override {

    if (!Name || !pNodeId)
      return E_POINTER;

    *pNodeId = (UINT)-1;

    auto it = Data.FullyResolvedToNodeId.find(Name);

    if (it == Data.FullyResolvedToNodeId.end())
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
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_SHADER_INPUT_BIND_DESC1 *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = Data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER)
      return E_INVALIDARG;

    return GetResourceBindingDesc(node.GetLocalId(), pDesc);
  }

  STDMETHOD(GetEnumDescByName)
  (THIS_ _In_ LPCSTR Name, _Out_ D3D12_HLSL_ENUM_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    UINT nodeId;
    IFR(GetNodeByName(Name, &nodeId));

    const DxcHLSLNode &node = Data.Nodes[nodeId];

    if (node.IsFwdDeclare())
      return E_UNEXPECTED;

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

    const DxcHLSLNode &node = Data.Nodes[nodeId];

    if (node.IsFwdDeclare())
      return E_UNEXPECTED;

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

    const DxcHLSLNode &node = Data.Nodes[nodeId];

    if (node.GetNodeType() != D3D12_HLSL_NODE_TYPE_FUNCTION)
      return E_INVALIDARG;

    return GetFunctionDesc(node.GetLocalId(), pDesc);
  }

  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer *, GetConstantBufferByName)
  (THIS_ _In_ LPCSTR Name) override {

    if (!Name)
      return &g_InvalidSRConstantBuffer;

    auto it = NameToConstantBuffers.find(Name);

    if (it == NameToConstantBuffers.end())
      return &g_InvalidSRConstantBuffer;

    return &ConstantBuffers[it->second];
  }

  STDMETHOD(GetStructTypeByName)
  (THIS_ _In_ LPCSTR Name,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (!Name)
      return E_POINTER;

    if (!(Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO))
      return E_INVALIDARG;

    auto it = NameToNonFwdIds[int(FwdDeclType::STRUCT)].find(Name);

    if (it == NameToNonFwdIds[int(FwdDeclType::STRUCT)].end())
      return E_INVALIDARG;

    *ppType = &Types[it->second];
    return S_OK;
  }

  STDMETHOD(GetUnionTypeByName)
  (THIS_ _In_ LPCSTR Name,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (!Name)
      return E_POINTER;

    if (!(Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO))
      return E_INVALIDARG;

    auto it = NameToNonFwdIds[int(FwdDeclType::UNION)].find(Name);

    if (it == NameToNonFwdIds[int(FwdDeclType::UNION)].end())
      return E_INVALIDARG;

    *ppType = &Types[it->second];
    return S_OK;
  }
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

bool IsAbsoluteOrCurDirRelative(const llvm::Twine &T) {
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
    if (IsAbsoluteOrCurDirRelative(A->getValue())) {
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

  //TODO: if (opts.RWOpt.ReflectHLSLScopes)
  //  reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_SCOPES;
  //
  //TODO: if (opts.RWOpt.ReflectHLSLVariables)
  //  reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_VARIABLES;

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
                                      hlsl::options::HlslFlags::RewriteOption,  //TODO: Change to other option
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
