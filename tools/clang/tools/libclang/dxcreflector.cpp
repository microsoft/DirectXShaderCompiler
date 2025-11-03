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

#include <sstream>

using namespace llvm;
using namespace clang;
using namespace hlsl;

namespace hlsl {

[[nodiscard]] DxcReflectionError DxcHLSLReflectionDataFromAST(DxcHLSLReflectionData &Result,
                                  clang::CompilerInstance &Compiler,
                                  clang::TranslationUnitDecl &Ctx,
                                  uint32_t AutoBindingSpace,
                                  D3D12_HLSL_REFLECTION_FEATURE Features,
                                  bool DefaultRowMaj);
}

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

  std::string m_NameUnderlying;
  std::string m_NameDisplay;
  std::vector<std::string> m_MemberNames;
  std::unordered_map<std::string, uint32_t> m_NameToMemberId;
  std::vector<CHLSLReflectionType *> m_MemberTypes;
  CHLSLReflectionType *m_pBaseClass;
  std::vector<CHLSLReflectionType *> m_Interfaces;

  const DxcHLSLReflectionData *m_Data;
  uint32_t m_TypeId;
  uint32_t m_ElementsUnderlying;
  uint32_t m_ElementsDisplay;
  D3D12_ARRAY_DESC m_ArrayDescUnderlying;
  D3D12_ARRAY_DESC m_ArrayDescDisplay;

  void InitializeArray(const DxcHLSLReflectionData &Data,
                       D3D12_ARRAY_DESC &desc, uint32_t &elements,
                       const DxcHLSLArrayOrElements &arrElem) {

    if (arrElem.IsArray()) {

      elements = arrElem.Is1DArray() ? arrElem.Get1DElements() : 1;

      if (arrElem.IsMultiDimensionalArray()) {

        const DxcHLSLArray &arr =
            Data.Arrays[arrElem.GetMultiDimensionalArrayId()];

        desc.ArrayDims = arr.ArrayElem();

        for (uint32_t i = 0; i < arr.ArrayElem(); ++i) {
          uint32_t len = Data.ArraySizes[arr.ArrayStart() + i];
          elements *= len;
          desc.ArrayLengths[i] = len;
        }
      }

      else {
        desc.ArrayDims = 1;
        desc.ArrayLengths[0] = arrElem.Get1DElements();
      }
    }
  }

public:

  STDMETHOD(IsEqual)(ID3D12ShaderReflectionType *pType) override {
    return (this == pType) ? S_OK : S_FALSE;
  }

  STDMETHOD(IsOfType)(ID3D12ShaderReflectionType *pType) override {

    if (this == pType)
      return S_OK;

    if (m_pBaseClass)
      return m_pBaseClass->IsOfType(pType);

    return S_FALSE;
  }

  STDMETHOD(GetArrayDesc)(THIS_ _Out_ D3D12_ARRAY_DESC *pArrayDesc) override {

    if (!pArrayDesc)
      return E_POINTER;

    *pArrayDesc = m_ArrayDescUnderlying;
    return S_OK;
  }

  STDMETHOD(GetDisplayArrayDesc)(THIS_ _Out_ D3D12_ARRAY_DESC *pArrayDesc) override {

    if (!pArrayDesc)
      return E_POINTER;

    *pArrayDesc = m_ArrayDescDisplay;
    return S_OK;
  }

  HRESULT Initialize(
      const DxcHLSLReflectionData &Data, uint32_t TypeId,
      std::vector<CHLSLReflectionType> &Types /* Only access < TypeId*/) {

    m_TypeId = TypeId;
    m_ElementsUnderlying = 0;
    m_ElementsDisplay = 0;
    m_Data = &Data;

    const DxcHLSLType &type = Data.Types[TypeId];

    ZeroMemoryToOut(&m_ArrayDescUnderlying);
    ZeroMemoryToOut(&m_ArrayDescDisplay);

    InitializeArray(Data, m_ArrayDescUnderlying, m_ElementsUnderlying,
                    type.GetUnderlyingArray());

    bool hasNames = Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

    if (hasNames) {
      DxcHLSLTypeSymbol sym = Data.TypeSymbols[TypeId];
      m_NameUnderlying = Data.Strings[sym.UnderlyingNameId];
      m_NameDisplay = Data.Strings[sym.DisplayNameId];
      InitializeArray(Data, m_ArrayDescDisplay, m_ElementsDisplay, sym.DisplayArray);
    }

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

    if (type.GetBaseClass() != uint32_t(-1))
      m_pBaseClass = &Types[type.GetBaseClass()];

    uint32_t interfaceCount = type.GetInterfaceCount();

    m_Interfaces.resize(interfaceCount);

    for (uint32_t i = 0; i < interfaceCount; ++i)
      m_Interfaces[i] = &Types[Data.TypeList[type.GetInterfaceStart() + i]];

    return S_OK;
  }

  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    const DxcHLSLType &type = m_Data->Types[m_TypeId];

    *pDesc = D3D12_SHADER_TYPE_DESC {

      type.GetClass(),
      type.GetType(),
      type.GetRows(),
      type.GetColumns(),

      m_ElementsUnderlying,
      uint32_t(m_MemberTypes.size()),
      0,                //TODO: Offset if we have one
      m_NameUnderlying.c_str()
    };

    return S_OK;
  }

  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC1 *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    const DxcHLSLType &type = m_Data->Types[m_TypeId];

    GetDesc(&pDesc->Desc);
    pDesc->DisplayName = m_NameDisplay.c_str();
    pDesc->DisplayElements = m_ElementsDisplay;

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
    return uint32_t(m_Interfaces.size());
  }

  STDMETHOD(ImplementsInterface)(ID3D12ShaderReflectionType *pBase) override {

    for (CHLSLReflectionType *interf : m_Interfaces)
      if (pBase == interf)
        return S_OK;

    return S_FALSE;
  }

  STDMETHOD_(ID3D12ShaderReflectionType *, GetInterfaceByIndex)
  (UINT uIndex) override {

    if (uIndex >= m_Interfaces.size())
      return nullptr;

    return m_Interfaces[uIndex];
  }
};

class CHLSLFunctionParameter final : public ID3D12FunctionParameterReflection {

protected:
  const DxcHLSLReflectionData *m_Data = nullptr;
  uint32_t m_NodeId = 0;

public:
  CHLSLFunctionParameter() = default;

  void Initialize(const DxcHLSLReflectionData &Data, uint32_t NodeId) {
    m_Data = &Data;
    m_NodeId = NodeId;
  }

  STDMETHOD(GetDesc)(THIS_ _Out_ D3D12_PARAMETER_DESC *pDesc) override {

    IFR(ZeroMemoryToOut(pDesc));

    const DxcHLSLNode &node = m_Data->Nodes[m_NodeId];

    LPCSTR semanticName =
        node.GetSemanticId() == uint32_t(-1)
            ? ""
            : m_Data->StringsNonDebug[node.GetSemanticId()].c_str();

    LPCSTR name =
        m_Data->Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO
            ? m_Data->Strings[m_Data->NodeSymbols[m_NodeId].GetNameId()].c_str()
            : "";

    const DxcHLSLParameter &param = m_Data->Parameters[node.GetLocalId()];
    const DxcHLSLType &type = m_Data->Types[param.TypeId];

    *pDesc =
        D3D12_PARAMETER_DESC{name,
                             semanticName,
                             type.GetType(),
                             type.GetClass(),
                             type.GetRows(),
                             type.GetColumns(),
                             D3D_INTERPOLATION_MODE(param.InterpolationMode),
                             D3D_PARAMETER_FLAGS(param.Flags)};

    return S_OK;
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

      m_ReflectionName = Data.Strings[Data.NodeSymbols[NodeId].GetNameId()];

      bool isCBuffer = reg.GetType() == D3D_SIT_CBUFFER;

      if (m_ReflectionName.size() && !isCBuffer) {

        uint32_t arrayDims = reg.GetArrayId() != uint32_t(-1)
                                 ? Data.Arrays[reg.GetArrayId()].ArrayElem()
                                 : (reg.GetBindCount() > 1 ? 1 : 0);

        for (unsigned i = 0; i < arrayDims; ++i)
          m_ReflectionName += "[0]";
      }
    }

    else
      m_ReflectionName.clear();

    m_Data = &Data;
    m_ChildCount = uint32_t(children.size());
    m_BufferType = m_Data->Buffers[reg.GetBufferId()].Type;

    m_VariablesByName.clear();
    m_Variables.resize(children.size());

    for (uint32_t i = 0, j = 0; i < m_ChildCount; ++i) {

      uint32_t childId = children[i];

      std::string name;

      if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
        name = Data.Strings[Data.NodeSymbols[childId].GetNameId()];

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
        name = Data.Strings[Data.NodeSymbols[childId].GetNameId()];

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
      INTERFACE,
      COUNT
  };

  std::vector<uint32_t> NonFwdIds[int(FwdDeclType::COUNT)];

  std::unordered_map<std::string, uint32_t>
      NameToNonFwdIds[int(FwdDeclType::COUNT)];

  std::vector<uint32_t> NodeToParameterId;
  std::vector<CHLSLFunctionParameter> FunctionParameters;

  DxcHLSLReflection() = default;

  void Finalize() {

    Data.GenerateNameLookupTable();

    ChildCountsNonRecursive.resize(Data.Nodes.size());
    ChildrenNonRecursive.clear();

    bool hasSymbols = Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;
    std::vector<uint32_t> globalVars;

    NodeToParameterId.resize(Data.Nodes.size());

    std::vector<uint32_t> functionParameters;
    functionParameters.reserve(Data.Nodes.size() / 4);

    for (uint32_t i = 0; i < uint32_t(Data.Nodes.size()); ++i) {

      const DxcHLSLNode &node = Data.Nodes[i];

      if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_VARIABLE &&
          !node.GetParentId())
        globalVars.push_back(i);

      if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_PARAMETER) {
        NodeToParameterId[i] = uint32_t(functionParameters.size());
        functionParameters.push_back(i);
      }

      // Filter out fwd declarations for structs, unions, interfaces, functions, enums

      if (!node.IsFwdDeclare()) {

        FwdDeclType type = FwdDeclType::COUNT;

        switch (node.GetNodeType()) {

        case D3D12_HLSL_NODE_TYPE_STRUCT:
          type = FwdDeclType::STRUCT;
          break;

        case D3D12_HLSL_NODE_TYPE_UNION:
          type = FwdDeclType::UNION;
          break;

        case D3D12_HLSL_NODE_TYPE_INTERFACE:
          type = FwdDeclType::INTERFACE;
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
                           [Data.Strings[Data.NodeSymbols[i].GetNameId()]] =
                               typeId;

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

    FunctionParameters.resize(functionParameters.size());

    for (uint32_t i = 0; i < uint32_t(functionParameters.size()); ++i)
      FunctionParameters[i].Initialize(Data, functionParameters[i]);

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

    NodeToParameterId = std::move(moved.NodeToParameterId);
    FunctionParameters = std::move(moved.FunctionParameters);

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
              uint32_t(NonFwdIds[int(FwdDeclType::UNION)].size()),
              uint32_t(NonFwdIds[int(FwdDeclType::INTERFACE)].size())};

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
            ? Data.Strings[Data.NodeSymbols[reg.GetNodeId()].GetNameId()]
                  .c_str()
            : "";

    if (reg.GetBindCount() > 1) {

      if (reg.GetArrayId() != uint32_t(-1)) {

        const DxcHLSLArray &arr = Data.Arrays[reg.GetArrayId()];

        pDesc->ArrayInfo.ArrayDims = arr.ArrayElem();

        for (uint32_t i = 0; i < pDesc->ArrayInfo.ArrayDims; ++i)
          pDesc->ArrayInfo.ArrayLengths[i] =
              Data.ArraySizes[arr.ArrayStart() + i];
      }

      else {
        pDesc->ArrayInfo.ArrayDims = 1;
        pDesc->ArrayInfo.ArrayLengths[0] = reg.GetBindCount();
      }
    }

    pDesc->Desc = D3D12_SHADER_INPUT_BIND_DESC{
        name, reg.GetType(),
        uint32_t(-1), // Invalid bindPoint, depending on backend we might
                      // want to change it
        reg.GetBindCount(),

        reg.GetFlags(), reg.GetReturnType(),
        reg.GetDimension(),
        uint32_t(-1), // Also no valid data depending on backend
        uint32_t(-1), // Invalid space (see bindPoint ^)
        reg.GetNodeId()};

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
            ? Data.Strings[Data.NodeSymbols[enm.NodeId].GetNameId()].c_str()
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
            ? Data.Strings[Data.NodeSymbols[enm.NodeId].GetNameId()].c_str()
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
            ? Data.Strings[Data.NodeSymbols[func.GetNodeId()].GetNameId()]
                  .c_str()
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
            ? Data.Strings[Data.NodeSymbols[NodeId].GetNameId()].c_str()
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

    LPCSTR semantic = "";

    if (node.GetSemanticId() != uint32_t(-1))
      semantic = Data.StringsNonDebug[node.GetSemanticId()].c_str();

    *pDesc = D3D12_HLSL_NODE{name,
                             semantic,
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

  // Use D3D_RETURN_PARAMETER_INDEX to get description of the return value.
  STDMETHOD_(ID3D12FunctionParameterReflection *, GetFunctionParameter)
  (THIS_ _In_ UINT FunctionIndex, THIS_ _In_ INT ParameterIndex) override {

    if (FunctionIndex >= Data.Functions.size())
      return nullptr;

    const DxcHLSLFunction &func = Data.Functions[FunctionIndex];

    if (ParameterIndex == D3D_RETURN_PARAMETER_INDEX) {

      if (!func.HasReturn())
        return nullptr;

      uint32_t parameterId =
          NodeToParameterId[func.GetNodeId() + 1 + func.GetNumParameters()];

      return &FunctionParameters[parameterId];
    }

    if (uint32_t(ParameterIndex) >= func.GetNumParameters())
      return nullptr;

    uint32_t parameterId =
        NodeToParameterId[func.GetNodeId() + 1 + ParameterIndex];
    return &FunctionParameters[parameterId];
  }

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

  STDMETHOD(GetInterfaceTypeByIndex)
  (THIS_ _In_ UINT Index,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (Index >= NonFwdIds[int(FwdDeclType::INTERFACE)].size())
      return E_INVALIDARG;

    *ppType = &Types[NonFwdIds[int(FwdDeclType::INTERFACE)][Index]];
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
        Data.Strings[Data.Sources[nodeSymbol.GetFileSourceId()]].c_str(),
        nodeSymbol.GetSourceLineStart(), nodeSymbol.GetSourceLineCount(),
        nodeSymbol.GetSourceColumnStart(), nodeSymbol.GetSourceColumnEnd()};

    return S_OK;
  }

  //Helper for conversion between symbol names

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

  STDMETHOD(GetInterfaceTypeByName)
  (THIS_ _In_ LPCSTR Name,
   _Outptr_ ID3D12ShaderReflectionType **ppType) override {

    IFR(ZeroMemoryToOut(ppType));

    if (!Name)
      return E_POINTER;

    if (!(Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO))
      return E_INVALIDARG;

    auto it = NameToNonFwdIds[int(FwdDeclType::INTERFACE)].find(Name);

    if (it == NameToNonFwdIds[int(FwdDeclType::INTERFACE)].end())
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
  compiler.getLangOpts().HLSLVersion = opts.HLSLVersion;
  compiler.getLangOpts().PreserveUnknownAnnotations = true;
  compiler.getLangOpts().UseMinPrecision = !opts.Enable16BitTypes;
  compiler.getDiagnostics().setIgnoreAllWarnings(!opts.OutputWarnings);
  compiler.getCodeGenOpts().MainFileName = pMainFile;
  compiler.getLangOpts().HLSLProfile = compiler.getCodeGenOpts().HLSLProfile =
      opts.TargetProfile;

  PreprocessorOptions &PPOpts = compiler.getPreprocessorOpts();
  if (rewrite != nullptr) {
    if (llvm::MemoryBuffer *pMemBuf = rewrite->second) {
      compiler.getPreprocessorOpts().addRemappedFile(StringRef(pMainFile),
                                                     pMemBuf);
    }

    PPOpts.RemappedFilesKeepOriginalName = true;
  }

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

  if (opts.Defines.size()) {

    std::string defines =
        DefinesToString(opts.Defines.DefineVector.data(), opts.Defines.size());

    compiler.getPreprocessor().setPredefines(
        compiler.getPreprocessor().getPredefines() + defines);
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

//TODO: Print escape character

struct JsonWriter {

  std::stringstream ss;
  uint16_t indent = 0;
  uint16_t countCommaStack = 0;
  uint32_t needCommaStack = 0;

  void Indent() { ss << std::string(indent, '\t'); }
  void NewLine() { ss << "\n"; }

  void StartCommaStack() {
    ++countCommaStack;
    assert(countCommaStack < 32 && "countCommaStack out of bounds");
    needCommaStack &= ~(1u << countCommaStack);
  }

  void SetComma() { needCommaStack |= 1u << countCommaStack; }

  void EndCommaStack() {
    --countCommaStack;
    SetComma();
  }

  bool NeedsComma() { return (needCommaStack >> countCommaStack) & 1; }

  void BeginObj() {

    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }

    ss << "{\n";
    ++indent;
    StartCommaStack();
    Indent();
  }

  void EndObj() {
    ss << "\n";
    --indent;
    Indent();
    ss << "}";
    EndCommaStack();
  }

  void BeginArray(const char *Name) {

    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }

    if (Name) {
      StartCommaStack();
      Key(Name);
      --countCommaStack;
    }

    ss << "[\n";
    ++indent;
    StartCommaStack();
    Indent();
  }

  void EndArray() {
    ss << "\n";
    --indent;
    Indent();
    ss << "]";
    EndCommaStack();
  }

  void Key(const char *Key) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << "\"" << Escape(Key) << "\": ";
  }

  void Value(std::string S) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << "\"" << Escape(S) << "\"";
    SetComma();
  }

  void Value(const char *S) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << "\"" << S << "\"";
    SetComma();
  }

  void ValueNull() {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << "null";
    SetComma();
  }

  void Value(uint64_t V) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << V;
    SetComma();
  }

  void Value(int64_t V) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << V;
    SetComma();
  }

  void Value(bool V) {
    if (NeedsComma()) {
      ss << ",\n";
      Indent();
    }
    ss << (V ? "true" : "false");
    SetComma();
  }

  static std::string Escape(const std::string &In) {
    std::string out;
    out.reserve(In.size());
    for (char c : In) {
      switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
      }
    }
    return out;
  }

  std::string str() const { return ss.str(); }

  struct ObjectScope {

    JsonWriter &W;
    bool NeedsEndCommaStack;

    ObjectScope(JsonWriter &W, const char *Name = nullptr)
        : W(W), NeedsEndCommaStack(Name) {

      if (Name) {
        W.Key(Name);
        W.StartCommaStack();
      }

      W.BeginObj();
    }

    ~ObjectScope() {
      W.EndObj();

      if (NeedsEndCommaStack)
        W.EndCommaStack();
    }
  };

  struct ArrayScope {
    JsonWriter &W;
    ArrayScope(JsonWriter &W, const char *Name = nullptr) : W(W) {
      W.BeginArray(Name);
    }
    ~ArrayScope() { W.EndArray(); }
  };
  
  void Object(const char *Name, const std::function<void()> &Body) {
    ObjectScope _(*this, Name);
    Body();
  }

  void Array(const char *Name, const std::function<void()> &Body) {
    ArrayScope _( *this, Name ); Body();
  }

  void StringField(const char *K, const std::string &V) {
    Key(K);
    StartCommaStack();
    Value(V);
    EndCommaStack();
  }

  void UIntField(const char *K, uint64_t V) {
    Key(K);
    StartCommaStack();
    Value(V);
    EndCommaStack();
  }

  void IntField(const char *K, int64_t V) {
    Key(K);
    StartCommaStack();
    Value(V);
    EndCommaStack();
  }

  void BoolField(const char *K, bool V) {
    Key(K);
    StartCommaStack();
    Value(V);
    EndCommaStack();
  }

  void NullField(const char *K) {
    Key(K);
    StartCommaStack();
    ValueNull();
    EndCommaStack();
  }
};

void PrintFeatures(D3D12_HLSL_REFLECTION_FEATURE Features, JsonWriter &Json) {
  Json.Array("Features", [Features, &Json] {
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS)
      Json.Value("Basics");
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS)
      Json.Value("Functions");
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES)
      Json.Value("Namespaces");
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES)
      Json.Value("UserTypes");
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)
      Json.Value("Scopes");
    if (Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
      Json.Value("Symbols");
  });
}

static const char *NodeTypeToString(D3D12_HLSL_NODE_TYPE Type) {

  static const char *arr[] = {"Register",
                              "Function",
                              "Enum",
                              "EnumValue",
                              "Namespace",
                              "Variable",
                              "Typedef",
                              "Struct",
                              "Union",
                              "StaticVariable",
                              "Interface",
                              "Parameter",
                              "If",
                              "Scope",
                              "Do",
                              "Switch",
                              "While",
                              "For",
                              "GroupsharedVariable"};

  return arr[uint32_t(Type)];
}

static const char *RegisterTypeToString(D3D_SHADER_INPUT_TYPE Type) {

  static const char *arr[] = {"cbuffer",
                              "tbuffer",
                              "Texture",
                              "SamplerState",
                              "RWTexture",
                              "StructuredBuffer",
                              "RWStructuredBuffer",
                              "ByteAddressBuffer",
                              "RWByteAddressBuffer",
                              "AppendStructuredBuffer",
                              "ConsumeStructuredBuffer",
                              "(Append/Consume)StructuredBuffer",
                              "RaytracingAccelerationStructure",
                              "FeedbackTexture"};

  return arr[uint32_t(Type)];
}

static const char *DimensionTypeToString(D3D_SRV_DIMENSION Type) {

  static const char *arr[] = {
      "Unknown",   "Buffer",         "Texture1D",        "Texture1DArray",
      "Texture2D", "Texture2DArray", "Texture2DMS",      "Texture2DMSArray",
      "Texture3D", "TextureCube",    "TextureCubeArray", "BufferEx"};

  return arr[uint32_t(Type)];
}

static const char *ReturnTypeToString(D3D_RESOURCE_RETURN_TYPE Type) {

  static const char *arr[] = {"unknown", "unorm", "snorm",  "sint",     "uint",
                              "float",   "mixed", "double", "continued"};

  return arr[uint32_t(Type)];
}

static std::string EnumTypeToString(D3D12_HLSL_ENUM_TYPE Type) {

  static const char *arr[] = {
      "uint", "int", "uint64_t", "int64_t", "uint16_t", "int16_t",
  };

  return arr[Type];
}

static std::string BufferTypeToString(D3D_CBUFFER_TYPE Type) {
  static const char *arr[] = {"cbuffer", "tbuffer", "undefined", "structured"};
  return arr[Type];
}

static std::string GetBuiltinTypeName(const DxcHLSLReflectionData &Refl,
                               const DxcHLSLType &Type) {

  std::string type = "<unknown>";

  if (Type.GetClass() != D3D_SVC_STRUCT && Type.GetClass() != D3D_SVC_INTERFACE_CLASS) {

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

    const char *ptr = arr[Type.GetType()];

    if (ptr)
      type = ptr;
  }

  switch (Type.GetClass()) {

  case D3D_SVC_MATRIX_ROWS:
  case D3D_SVC_VECTOR:

    type += std::to_string(Type.GetColumns());

    if (Type.GetClass() == D3D_SVC_MATRIX_ROWS)
      type += "x" + std::to_string(Type.GetRows());

    break;

  case D3D_SVC_MATRIX_COLUMNS:
    type += std::to_string(Type.GetRows()) + "x" +
            std::to_string(Type.GetColumns());
    break;
  }

  return type;
}

static void FillArraySizes(DxcHLSLReflectionData &Reflection,
                           DxcHLSLArrayOrElements Elements,
                           std::vector<uint32_t> &Array) {

  if (!Elements.IsArray())
    return;

  if (Elements.Is1DArray()) {
    Array.push_back(Elements.Get1DElements());
    return;
  }

  const DxcHLSLArray &arr =
      Reflection.Arrays[Elements.GetMultiDimensionalArrayId()];

  for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
    Array.push_back(Reflection.ArraySizes[arr.ArrayStart() + i]);
}

//Verbose and all members are slightly different;
//Verbose will still print fields even if they aren't relevant,
// while all members will not silence important info but that might not matter for human readability
static void PrintNode(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                 uint32_t NodeId, bool IsVerbose, bool AllRelevantMembers) {

  DxcHLSLNode &node = Reflection.Nodes[NodeId];

  JsonWriter::ObjectScope nodeRoot(Json);

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (AllRelevantMembers || IsVerbose || !hasSymbols)
    Json.UIntField("NodeId", NodeId);

  Json.StringField("NodeType", NodeTypeToString(node.GetNodeType()));

  if (IsVerbose || AllRelevantMembers) {
    Json.UIntField("NodeTypeId", node.GetNodeType());
    Json.UIntField("LocalId", node.GetLocalId());
    Json.IntField("ParentId", node.GetParentId() == uint16_t(-1)
                                  ? -1
                                  : int64_t(node.GetParentId()));
  }

  if (IsVerbose || (node.GetChildCount() && AllRelevantMembers)) {
    Json.UIntField("ChildCount", node.GetChildCount());
    Json.UIntField("ChildStart", NodeId + 1);
  }

  if (node.GetSemanticId() != uint32_t(-1) || IsVerbose)
    Json.StringField("Semantic",
                     node.GetSemanticId() != uint32_t(-1)
                         ? Reflection.StringsNonDebug[node.GetSemanticId()]
                         : "");

  if (IsVerbose || (AllRelevantMembers && node.GetSemanticId() != uint32_t(-1)))
    Json.IntField("SemanticId", node.GetSemanticId() == uint32_t(-1) ? -1 : int64_t(node.GetSemanticId()));

  if (IsVerbose || (AllRelevantMembers && node.GetAnnotationCount())) {
    Json.UIntField("AnnotationStart", node.GetAnnotationStart());
    Json.UIntField("AnnotationCount", node.GetAnnotationCount());
  }

  if (node.GetAnnotationCount() || IsVerbose)
    Json.Array("Annotations", [&Reflection, &Json, node] {
      for (uint32_t i = 0; i < node.GetAnnotationCount(); ++i) {

        const DxcHLSLAnnotation &annot =
            Reflection.Annotations[node.GetAnnotationStart() + i];

        std::string name =
            Reflection.StringsNonDebug[annot.GetStringNonDebug()];

        if (annot.GetIsBuiltin())
          name = "[" + name + "]";
        else
          name = "[[" + name + "]]";

        Json.Value(name);
      }
    });

  if ((node.IsFwdBckDefined() || node.IsFwdDeclare()) || IsVerbose) {

    Json.BoolField("IsFwdDeclare", node.IsFwdDeclare());
    Json.BoolField("IsFwdBackDefined", node.IsFwdBckDefined());

    if (node.IsFwdBckDefined() || IsVerbose)
      Json.IntField("FwdBack",
                    !node.IsFwdBckDefined() ? -1 : int64_t(node.GetFwdBck()));
  }

  if (hasSymbols) {
  
    Json.Object("Symbol", [&Reflection, &Json, NodeId, IsVerbose,
                           AllRelevantMembers] {
      DxcHLSLNodeSymbol &sym = Reflection.NodeSymbols[NodeId];
      Json.StringField("Name", Reflection.Strings[sym.GetNameId()]);

      if (IsVerbose || AllRelevantMembers)
        Json.UIntField("NameId", sym.GetNameId());

      if (sym.HasFileSource()) {

        Json.StringField(
            "Source",
            Reflection.Strings[Reflection.Sources[sym.GetFileSourceId()]]);

        if (IsVerbose || AllRelevantMembers)
          Json.UIntField("SourceId", sym.GetFileSourceId());

        Json.UIntField("LineId", sym.GetSourceLineStart());
        Json.UIntField("LineCount", sym.GetSourceLineCount());

        Json.UIntField("ColumnStart", sym.GetSourceColumnStart());
        Json.UIntField("ColumnEnd", sym.GetSourceColumnEnd());

      } else if (IsVerbose) {

        Json.StringField("Source", "");
        Json.IntField("SourceId", -1);

        Json.IntField("LineId", -1);
        Json.UIntField("LineCount", 0);

        Json.IntField("ColumnStart", -1);
        Json.UIntField("ColumnEnd", 0);
      }
    });

  } else if (IsVerbose)
    Json.NullField("Symbol");
}

static void PrintRegister(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                          uint32_t RegisterId, bool IsVerbose,
                          bool AllRelevantMembers) {

  DxcHLSLRegister &reg = Reflection.Registers[RegisterId];

  JsonWriter::ObjectScope nodeRoot(Json);

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (IsVerbose || AllRelevantMembers || !hasSymbols) {
    Json.UIntField("RegisterId", RegisterId);
    Json.UIntField("NodeId", reg.GetNodeId());
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection
            .Strings[Reflection.NodeSymbols[reg.GetNodeId()].GetNameId()]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  Json.StringField("RegisterType", RegisterTypeToString(reg.GetType()));

  if (reg.GetDimension() != D3D_SRV_DIMENSION_UNKNOWN || IsVerbose)
    Json.StringField("Dimension", DimensionTypeToString(reg.GetDimension()));

  if (reg.GetReturnType() || IsVerbose)
    Json.StringField("ReturnType", ReturnTypeToString(reg.GetReturnType()));

  if (reg.GetBindCount() > 1 || IsVerbose)
    Json.UIntField("BindCount", reg.GetBindCount());

  if (reg.GetArrayId() != uint32_t(-1) || IsVerbose) {

    if (IsVerbose || AllRelevantMembers)
      Json.UIntField("ArrayId", reg.GetArrayId());

    Json.Array("ArraySize", [&Reflection, &reg, &Json]() {

      if (reg.GetArrayId() == uint32_t(-1))
        return;

      const DxcHLSLArray &arr = Reflection.Arrays[reg.GetArrayId()];

      for (uint32_t i = 0; i < uint32_t(arr.ArrayElem()); ++i)
        Json.Value(uint64_t(Reflection.ArraySizes[arr.ArrayStart() + i]));
    });
  }

  bool isBuffer = true;

  switch (reg.GetType()) {
  case D3D_SIT_TEXTURE:
  case D3D_SIT_SAMPLER:
  case D3D_SIT_UAV_RWTYPED:
  case D3D_SIT_RTACCELERATIONSTRUCTURE:
  case D3D_SIT_UAV_FEEDBACKTEXTURE:
    isBuffer = false;
    break;
  }

  if (isBuffer || IsVerbose)
    Json.UIntField("BufferId", reg.GetBufferId());

  if (reg.GetFlags() || IsVerbose) {

    Json.Array("Flags", [&reg, &Json]() {

      uint32_t flag = reg.GetFlags();

      if (flag & D3D_SIF_USERPACKED)
        Json.Value("UserPacked");

      if (flag & D3D_SIF_COMPARISON_SAMPLER)
        Json.Value("ComparisonSampler");

      if (flag & D3D_SIF_TEXTURE_COMPONENT_0)
        Json.Value("TextureComponent0");

      if (flag & D3D_SIF_TEXTURE_COMPONENT_1)
        Json.Value("TextureComponent1");

      if (flag & D3D_SIF_UNUSED)
        Json.Value("Unused");
    });
  }
}

static void PrintTypeName(DxcHLSLReflectionData &Reflection, uint32_t TypeId,
                          bool HasSymbols, bool IsVerbose,
                          bool AllRelevantMembers, JsonWriter &Json,
                          const char *NameForTypeName = "Name") {

  if (AllRelevantMembers || IsVerbose || !HasSymbols)
    Json.UIntField("TypeId", TypeId);

  std::string name;
  std::vector<uint32_t> arraySizes;

  std::string underlyingName;
  std::vector<uint32_t> underlyingArraySizes;

  const DxcHLSLType &type = Reflection.Types[TypeId];

  if (!HasSymbols) {
    name = GetBuiltinTypeName(Reflection, type);
    FillArraySizes(Reflection, type.GetUnderlyingArray(), arraySizes);
  }

  else {
    const DxcHLSLTypeSymbol &symbol = Reflection.TypeSymbols[TypeId];
    name = Reflection.Strings[symbol.DisplayNameId];
    FillArraySizes(Reflection, symbol.DisplayArray, arraySizes);
    underlyingName = Reflection.Strings[symbol.UnderlyingNameId];
    FillArraySizes(Reflection, type.GetUnderlyingArray(), underlyingArraySizes);
  }

  if (name.size() || IsVerbose)
    Json.StringField(NameForTypeName, name);

  if (arraySizes.size() || IsVerbose)
    Json.Array("ArraySize", [&arraySizes, &Json]() {
      for (uint32_t i : arraySizes)
        Json.Value(uint64_t(i));
    });

  if ((underlyingName.size() && underlyingName != name) || IsVerbose)
    Json.StringField("UnderlyingName", underlyingName);

  if ((underlyingArraySizes.size() && underlyingArraySizes != arraySizes) || IsVerbose)
    Json.Array("UnderlyingArraySize", [&underlyingArraySizes, &Json]() {
      for (uint32_t i : underlyingArraySizes)
        Json.Value(uint64_t(i));
    });
}

static void PrintType(DxcHLSLReflectionData &Reflection, uint32_t TypeId,
                      bool HasSymbols, bool IsVerbose, bool AllRelevantMembers,
                      JsonWriter &Json) {

  const DxcHLSLType &type = Reflection.Types[TypeId];

  JsonWriter::ObjectScope nodeRoot(Json);
  PrintTypeName(Reflection, TypeId, HasSymbols, IsVerbose, AllRelevantMembers,
                Json);

  if (type.GetBaseClass() != uint32_t(-1))
    Json.Object("BaseClass", [&Reflection, &Json, &type, HasSymbols, IsVerbose,
                              AllRelevantMembers]() {
      PrintTypeName(Reflection, type.GetBaseClass(), HasSymbols, IsVerbose,
                    AllRelevantMembers, Json);
    });

  else if (IsVerbose)
    Json.NullField("BaseClass");

  if (type.GetInterfaceCount())
    Json.Array("Interfaces", [&Reflection, &Json, &type, HasSymbols, IsVerbose,
                              AllRelevantMembers]() {
      for (uint32_t i = 0; i < uint32_t(type.GetInterfaceCount()); ++i) {
        uint32_t interfaceId = type.GetInterfaceStart() + i;
        JsonWriter::ObjectScope nodeRoot(Json);
        PrintTypeName(Reflection, Reflection.TypeList[interfaceId], HasSymbols,
                      IsVerbose, AllRelevantMembers, Json);
      }
    });

  else if (IsVerbose)
    Json.NullField("Interfaces");

  if (type.GetMemberCount())
    Json.Array("Members", [&Reflection, &Json, &type, HasSymbols, IsVerbose,
                           AllRelevantMembers]() {
      for (uint32_t i = 0; i < uint32_t(type.GetMemberCount()); ++i) {

        uint32_t memberId = type.GetMemberStart() + i;
        JsonWriter::ObjectScope nodeRoot(Json);

        if (HasSymbols) {
          Json.StringField(
              "Name", Reflection.Strings[Reflection.MemberNameIds[memberId]]);
          Json.UIntField("NameId", Reflection.MemberNameIds[memberId]);
        }

        PrintTypeName(Reflection, Reflection.MemberTypeIds[memberId],
                      HasSymbols, IsVerbose, AllRelevantMembers, Json,
                      "TypeName");
      }
    });

  else if (IsVerbose)
    Json.NullField("Members");
}

static void PrintParameter(DxcHLSLReflectionData &Reflection, uint32_t TypeId,
                           bool HasSymbols, bool IsVerbose, JsonWriter &Json,
                           uint32_t SemanticId, uint8_t InterpolationMode,
                           uint8_t Flags, bool AllRelevantMembers) {

  PrintTypeName(Reflection, TypeId, HasSymbols, IsVerbose, AllRelevantMembers, Json, "TypeName");

  if (SemanticId != uint32_t(-1) || IsVerbose)
    Json.StringField("Semantic", SemanticId == uint32_t(-1)
                                     ? ""
                                     : Reflection.StringsNonDebug[SemanticId]);

  if (IsVerbose || (AllRelevantMembers && SemanticId != uint32_t(-1)))
    Json.IntField("SemanticId",
                  SemanticId == uint32_t(-1) ? -1 : int64_t(SemanticId));

  if ((Flags & (D3D_PF_IN | D3D_PF_OUT)) == (D3D_PF_IN | D3D_PF_OUT))
    Json.StringField("Access", "inout");

  else if (Flags & D3D_PF_IN)
    Json.StringField("Access", "in");

  else if (Flags & D3D_PF_OUT)
    Json.StringField("Access", "out");

  else if (IsVerbose)
    Json.StringField("Access", "in");

  static const char *interpolationModes[] = {"Undefined",
                                             "Constant",
                                             "Linear",
                                             "LinearCentroid",
                                             "LinearNoperspective",
                                             "LinearNoperspectiveCentroid",
                                             "LinearSample",
                                             "LinearNoperspectiveSample"};

  if (InterpolationMode || IsVerbose)
    Json.StringField("Interpolation", interpolationModes[InterpolationMode]);
}

static void PrintFunction(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                          uint32_t FunctionId, bool IsVerbose,
                          bool AllRelevantMembers) {

  DxcHLSLFunction &func = Reflection.Functions[FunctionId];

  JsonWriter::ObjectScope nodeRoot(Json);

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (AllRelevantMembers || IsVerbose || !hasSymbols) {
    Json.UIntField("FunctionId", FunctionId);
    Json.UIntField("NodeId", func.GetNodeId());
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection
            .Strings[Reflection.NodeSymbols[func.GetNodeId()].GetNameId()]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  Json.BoolField("HasDefinition", func.HasDefinition());

  if (IsVerbose) {
    Json.UIntField("NumParameters", func.GetNumParameters());
    Json.BoolField("HasReturn", func.HasReturn());
  }

  Json.Object("Params", [&Reflection, &func, &Json, hasSymbols, IsVerbose,
                         AllRelevantMembers]() {
    for (uint32_t i = 0; i < uint32_t(func.GetNumParameters()); ++i) {

      uint32_t nodeId = func.GetNodeId() + 1 + i;
      const DxcHLSLNode &node = Reflection.Nodes[nodeId];
      uint32_t localId = node.GetLocalId();

      const DxcHLSLParameter &param = Reflection.Parameters[localId];
      std::string paramName =
          hasSymbols
              ? Reflection.Strings[Reflection.NodeSymbols[nodeId].GetNameId()]
              : std::to_string(i);

      Json.Object(paramName.c_str(), [&Reflection, &func, &Json, hasSymbols,
                                      IsVerbose, &param, &node,
                                      AllRelevantMembers]() {
        PrintParameter(Reflection, param.TypeId, hasSymbols, IsVerbose, Json,
                       node.GetSemanticId(), param.InterpolationMode,
                       param.Flags, AllRelevantMembers);
      });
    }
  });

  if (!func.HasReturn())
    Json.StringField("ReturnType", "void");

  else {

    const DxcHLSLNode &node =
        Reflection.Nodes[func.GetNodeId() + 1 + func.GetNumParameters()];
    const DxcHLSLParameter &param = Reflection.Parameters[node.GetLocalId()];

    Json.Object("ReturnType", [&Reflection, &func, &Json, hasSymbols, IsVerbose,
                               &param, &node, AllRelevantMembers]() {
      PrintParameter(Reflection, param.TypeId, hasSymbols, IsVerbose, Json,
                     node.GetSemanticId(), param.InterpolationMode, param.Flags,
                     AllRelevantMembers);
    });
  }
}

static void PrintEnumValue(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                           uint32_t ChildId, bool IsVerbose,
                           bool AllRelevantMembers) {

  const DxcHLSLNode &child = Reflection.Nodes[ChildId];

  const DxcHLSLEnumValue &val = Reflection.EnumValues[child.GetLocalId()];

  const DxcHLSLNode &parent = Reflection.Nodes[child.GetParentId()];
  const DxcHLSLEnumDesc &enm = Reflection.Enums[parent.GetLocalId()];

  switch (enm.Type) {
  case D3D12_HLSL_ENUM_TYPE_INT:
  case D3D12_HLSL_ENUM_TYPE_INT64_T:
  case D3D12_HLSL_ENUM_TYPE_INT16_T:
    Json.IntField("Value", val.Value);
    break;

  default:
    Json.UIntField("Value", uint64_t(val.Value));
  }

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (hasSymbols || IsVerbose)
    Json.StringField(
        "Name",
        !hasSymbols
            ? ""
            : Reflection.Strings[Reflection.NodeSymbols[ChildId].GetNameId()]);
}

static void PrintEnum(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                      uint32_t EnumId, bool IsVerbose,
                      bool AllRelevantMembers) {

  JsonWriter::ObjectScope nodeRoot(Json);
  const DxcHLSLEnumDesc &enm = Reflection.Enums[EnumId];
  const DxcHLSLNode &node = Reflection.Nodes[enm.NodeId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (AllRelevantMembers || IsVerbose || !hasSymbols) {
    Json.UIntField("EnumId", EnumId);
    Json.UIntField("NodeId", enm.NodeId);
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection.Strings[Reflection.NodeSymbols[enm.NodeId].GetNameId()]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  Json.StringField("EnumType", EnumTypeToString(enm.Type));

  Json.Array("Values", [&Json, &node, &enm, hasSymbols, &Reflection, IsVerbose,
                        AllRelevantMembers]() {
    for (uint32_t i = 0; i < node.GetChildCount(); ++i) {

      uint32_t childId = enm.NodeId + 1 + i;

      JsonWriter::ObjectScope valueRoot(Json);

      if (!hasSymbols || AllRelevantMembers || IsVerbose)
        Json.UIntField("ValueId", i);

      PrintEnumValue(Json, Reflection, childId, IsVerbose, AllRelevantMembers);
    }
  });
}

static void PrintAnnotation(JsonWriter &Json, DxcHLSLReflectionData &Reflection,
                            const DxcHLSLAnnotation &Annot) {
  Json.StringField("Contents",
                   Reflection.StringsNonDebug[Annot.GetStringNonDebug()]);
  Json.StringField("Type", Annot.GetIsBuiltin() ? "Builtin" : "User");
}

static void PrintBufferMemberMember(DxcHLSLReflectionData &Reflection,
                                    bool HasSymbols, bool IsVerbose,
                                    bool AllRelevantMembers, JsonWriter &Json,
                                    uint32_t ChildId, uint32_t MemberId) {

  uint32_t TypeId = Reflection.MemberTypeIds[MemberId];

  JsonWriter::ObjectScope nodeRoot(Json);

  if (IsVerbose || AllRelevantMembers || !HasSymbols)
    Json.UIntField("ChildId", ChildId);

  if (HasSymbols)
    Json.StringField("Name",
                     Reflection.Strings[Reflection.MemberNameIds[MemberId]]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  const DxcHLSLType &type = Reflection.Types[TypeId];

  PrintTypeName(Reflection, TypeId, HasSymbols, IsVerbose, AllRelevantMembers,
                Json, "TypeName");

  if (type.GetMemberCount())
    Json.Array("Children", [&type, &Reflection, &Json, HasSymbols, IsVerbose,
                            AllRelevantMembers]() {
      for (uint32_t i = 0; i < type.GetMemberCount(); ++i) {

        uint32_t memberId = type.GetMemberStart() + i;

        PrintBufferMemberMember(Reflection, HasSymbols, IsVerbose,
                                AllRelevantMembers, Json, i, memberId);
      }
    });
}

static uint32_t PrintBufferMember(DxcHLSLReflectionData &Reflection,
                                  uint32_t NodeId, bool HasSymbols,
                                  bool IsVerbose, bool AllRelevantMembers,
                                  JsonWriter &Json) {

  const DxcHLSLNode &node = Reflection.Nodes[NodeId];

  JsonWriter::ObjectScope root(Json);

  if (IsVerbose || AllRelevantMembers || !HasSymbols)
    Json.UIntField("NodeId", NodeId);

  if (HasSymbols)
    Json.StringField(
        "Name", Reflection.Strings[Reflection.NodeSymbols[NodeId].GetNameId()]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  PrintTypeName(Reflection, node.GetLocalId(), HasSymbols, IsVerbose,
                AllRelevantMembers, Json, "TypeName");

  const DxcHLSLType &type = Reflection.Types[node.GetLocalId()];

  if (type.GetMemberCount() || IsVerbose)
    Json.Array("Children", [&type, &Reflection, &Json, HasSymbols, IsVerbose,
                            AllRelevantMembers]() {
      for (uint32_t i = 0; i < type.GetMemberCount(); ++i)
        PrintBufferMemberMember(Reflection, HasSymbols, IsVerbose,
                                AllRelevantMembers, Json, i,
                                type.GetMemberStart() + i);
    });

  return node.GetChildCount();
}

static void PrintBuffer(DxcHLSLReflectionData &Reflection, uint32_t BufferId,
                      bool HasSymbols, bool IsVerbose, bool AllRelevantMembers,
                      JsonWriter &Json) {
    
  JsonWriter::ObjectScope nodeRoot(Json);
  const DxcHLSLBuffer &buf = Reflection.Buffers[BufferId];
  const DxcHLSLNode &node = Reflection.Nodes[buf.NodeId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (AllRelevantMembers || IsVerbose || !hasSymbols) {
    Json.UIntField("BufferId", BufferId);
    Json.UIntField("NodeId", buf.NodeId);
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection.Strings[Reflection.NodeSymbols[buf.NodeId].GetNameId()]);

  else if (IsVerbose)
    Json.StringField("Name", "");

  Json.StringField("Type", BufferTypeToString(buf.Type));

  if (node.GetChildCount() || IsVerbose)
    Json.Array("Children", [&node, &Reflection, &buf, &Json, HasSymbols,
                            IsVerbose, AllRelevantMembers]() {
      for (uint32_t i = 0; i < node.GetChildCount(); ++i)
        i += PrintBufferMember(Reflection, buf.NodeId + 1 + i, HasSymbols,
                               IsVerbose, AllRelevantMembers, Json);
    });
}

static void PrintStatement(DxcHLSLReflectionData &Reflection,
                           const DxcHLSLStatement &Stmt, JsonWriter &Json,
                           bool IsVerbose) {

  const DxcHLSLNode &node = Reflection.Nodes[Stmt.GetNodeId()];

  uint32_t nodesA = Stmt.GetNodeCount();
  uint32_t nodesB = node.GetChildCount() - nodesA - Stmt.HasConditionVar();

  Json.BoolField("HasConditionVar", Stmt.HasConditionVar());

  bool isIf = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_IF;

  if (isIf || IsVerbose)
    Json.BoolField("HasElse", Stmt.HasElse());

  if (nodesA || IsVerbose)
    Json.UIntField("NodesA", nodesA);

  if (nodesB || IsVerbose)
    Json.UIntField("NodesB", nodesB);
}

//IsHumanFriendly = false: Raw view of the real file data
//IsHumanFriendly = true:  Clean view that's relatively close to the real tree
static std::string ToJson(DxcHLSLReflectionData &Reflection,
                          bool IsHumanFriendly, bool IsVerbose) {

  JsonWriter json;

  {
    JsonWriter::ObjectScope root(json);

    // Features

    PrintFeatures(Reflection.Features, json);

    bool hasSymbols =
        Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

    // Print raw contents

    if (!IsHumanFriendly) {

      json.Array("Strings", [&Reflection, &json] {
        for (const std::string &s : Reflection.Strings)
          json.Value(s);
      });

      json.Array("StringsNonDebug", [&Reflection, &json] {
        for (const std::string &s : Reflection.StringsNonDebug)
          json.Value(s);
      });

      json.Array("Sources", [&Reflection, &json] {
        for (uint32_t id : Reflection.Sources)
          json.Value(Reflection.Strings[id]);
      });

      json.Array("SourcesAsId", [&Reflection, &json] {
        for (uint32_t id : Reflection.Sources)
          json.Value(uint64_t(id));
      });

      json.Array("Nodes", [&Reflection, &json, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Nodes.size()); ++i)
          PrintNode(json, Reflection, i, IsVerbose, true);
      });

      json.Array("Registers", [&Reflection, &json, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Registers.size()); ++i)
          PrintRegister(json, Reflection, i, IsVerbose, true);
      });

      json.Array("Functions", [&Reflection, &json, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Functions.size()); ++i)
          PrintFunction(json, Reflection, i, IsVerbose, true);
      });

      // Already referenced indirectly through other properties we printed
      // before, still printing it to allow consistency checks.
      // For pure pretty prints, you should use the human version.

      json.Array("Parameters", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Parameters.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);

          const DxcHLSLParameter &param = Reflection.Parameters[i];
          std::string paramName =
              hasSymbols
                  ? Reflection.Strings[Reflection.NodeSymbols[param.NodeId]
                                           .GetNameId()]
                  : std::to_string(i);

          json.StringField("ParamName", paramName);

          PrintParameter(Reflection, param.TypeId, hasSymbols, IsVerbose, json,
                         Reflection.Nodes[param.NodeId].GetSemanticId(),
                         param.InterpolationMode, param.Flags, true);
        }
      });

      json.Array("Enums", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Enums.size()); ++i)
          PrintEnum(json, Reflection, i, IsVerbose, true);
      });

      json.Array("EnumValues", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.EnumValues.size()); ++i) {
          JsonWriter::ObjectScope valueRoot(json);
          PrintEnumValue(json, Reflection, Reflection.EnumValues[i].NodeId,
                         IsVerbose, true);
        }
      });

      json.Array("Annotations", [&Reflection, &json, hasSymbols] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Annotations.size()); ++i) {
          const DxcHLSLAnnotation &annot = Reflection.Annotations[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.UIntField("StringId", annot.GetStringNonDebug());
          PrintAnnotation(json, Reflection, annot);
        }
      });

      json.Array("Arrays", [&Reflection, &json] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Arrays.size()); ++i) {
          const DxcHLSLArray &arr = Reflection.Arrays[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.UIntField("ArrayElem", arr.ArrayElem());
          json.UIntField("ArrayStart", arr.ArrayStart());
          json.Array("ArraySizes", [&Reflection, &json, &arr] {
            for (uint32_t i = 0; i < arr.ArrayElem(); ++i) {
              json.Value(uint64_t(Reflection.ArraySizes[arr.ArrayStart() + i]));
            }
          });
        }
      });

      json.Array("ArraySizes", [&Reflection, &json] {
        for (uint32_t id : Reflection.ArraySizes)
          json.Value(uint64_t(id));
      });

      json.Array("Members", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.MemberTypeIds.size());
             ++i) {

          JsonWriter::ObjectScope valueRoot(json);

          if (hasSymbols) {
            json.StringField("Name",
                             Reflection.Strings[Reflection.MemberNameIds[i]]);
            json.UIntField("NameId", Reflection.MemberNameIds[i]);
          }

          PrintTypeName(Reflection, Reflection.MemberTypeIds[i], hasSymbols,
                        IsVerbose, true, json, "TypeName");
        }
      });

      json.Array("TypeList", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t id : Reflection.TypeList) {
          JsonWriter::ObjectScope valueRoot(json);
          PrintTypeName(Reflection, id, hasSymbols, IsVerbose, true, json);
        }
      });

      json.Array("Types", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Types.size()); ++i)
          PrintType(Reflection, i, hasSymbols, IsVerbose, true, json);
      });

      json.Array("Buffers", [&Reflection, &json, hasSymbols, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Buffers.size()); ++i)
          PrintBuffer(Reflection, i, hasSymbols, IsVerbose, true, json);
      });

      json.Array("Statements", [&Reflection, &json, IsVerbose] {
        for (uint32_t i = 0; i < uint32_t(Reflection.Statements.size()); ++i) {

          const DxcHLSLStatement &stat = Reflection.Statements[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.StringField(
              "Type", NodeTypeToString(
                          Reflection.Nodes[stat.GetNodeId()].GetNodeType()));
          json.UIntField("NodeId", stat.GetNodeId());

          PrintStatement(Reflection, stat, json, IsVerbose);
        }
      });
    }

    else {
      // TODO: Print pretty tree for user
    }
  }

  return json.str();
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

  if (opts.ReflOpt.Basics)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_BASICS;

  if (opts.ReflOpt.Functions)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS;

  if (opts.ReflOpt.Namespaces)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES;

  if (opts.ReflOpt.UserTypes)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES;

  if (opts.ReflOpt.Scopes)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_SCOPES;

  if (!reflectMask)
    reflectMask = D3D12_HLSL_REFLECTION_FEATURE_ALL;

  if (!opts.ReflOpt.DisableSymbols)
    reflectMask |= D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  DxcHLSLReflectionData refl;
  
  if (DxcReflectionError err = DxcHLSLReflectionDataFromAST(
          refl, astHelper.compiler, *astHelper.tu, opts.AutoBindingSpace,
          reflectMask, opts.DefaultRowMajor)) {
    printf("DxcHLSLReflectionDataFromAST failed %s\n", err.err);     //TODO:
    return E_FAIL;
  }

  printf("%s\n", ToJson(refl, false, false).c_str());

  // Test serialization

  std::vector<std::byte> bytes;
  refl.Dump(bytes);

  DxcHLSLReflectionData deserialized;
  
  if (DxcReflectionError err = deserialized.Deserialize(bytes, true)) {
    printf("Deserialize failed %s\n", err.err); // TODO:
    return E_FAIL;
  }

  if (!(deserialized == refl)) {
    printf("Dump or Deserialize doesn't match\n");  //TODO:
    return E_FAIL;
  }

  printf("Reflection size: %" PRIu64 "\n", bytes.size());

  // Test stripping symbols

  refl.StripSymbols();

  printf("%s\n", ToJson(refl, false, false).c_str());

  refl.Dump(bytes);

  DxcHLSLReflectionData deserialized2;

  if (DxcReflectionError err = deserialized2.Deserialize(bytes, true)) {
    printf("Deserialize failed %s\n", err.err); // TODO:
    return E_FAIL;
  }

  if (!(deserialized2 == refl)) {
    printf("Dump or Deserialize doesn't match\n");      //TODO:
    return E_FAIL;
  }

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
                                      hlsl::options::HlslFlags::ReflectOption,
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

  HRESULT STDMETHODCALLTYPE FromBlob(IDxcBlob *data, IDxcHLSLReflection **ppReflection) override {
      
    if (!data || !data->GetBufferSize() || !ppReflection)
      return E_POINTER;

    std::vector<std::byte> bytes((const std::byte *)data->GetBufferPointer(),
                                 (const std::byte *)data->GetBufferPointer() +
                                     data->GetBufferSize());

    try {
      DxcHLSLReflectionData deserial;

      if (DxcReflectionError err = deserial.Deserialize(bytes, true))
        return E_FAIL;

      *ppReflection = new DxcHLSLReflection(std::move(deserial));
      return S_OK;
    } catch (...) {
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE ToBlob(IDxcHLSLReflection *reflection,
      IDxcBlob **ppResult) override {

    if (!reflection || !ppResult)
      return E_POINTER;

    DxcHLSLReflection *refl = dynamic_cast<DxcHLSLReflection *>(reflection);

    if (!refl)
      return E_UNEXPECTED;

    std::vector<std::byte> bytes;
    refl->Data.Dump(bytes);

    return DxcCreateBlobOnHeapCopy(bytes.data(), bytes.size(), ppResult);
  }
};

HRESULT CreateDxcReflector(REFIID riid, LPVOID *ppv) {
  CComPtr<DxcReflector> isense = DxcReflector::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(isense.p);
  return isense.p->QueryInterface(riid, ppv);
}
