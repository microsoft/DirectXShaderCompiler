///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflection.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <assert.h>

#include "d3d12shader.h"
#include "dxc/dxctools.h"

namespace clang {
class TranslationUnitDecl;
class CompilerInstance;
}

#pragma warning(disable : 4201)

namespace hlsl {

class DxcHLSLNode {

  uint32_t LocalIdParentLo;             //24 : 8

  union {
    uint32_t ParentHiAnnotationsType32;
    struct {
      uint16_t ParentHi;
      uint8_t Annotations;
      uint8_t Type;
    };
  };

  uint32_t ChildCountFwdBckLo;          //24 : 8

  union {
    uint32_t AnnotationStartFwdBckHi;
    struct {
      uint16_t AnnotationStart;
      uint16_t FwdBckHi;
    };
  };

  uint16_t SemanticId;
  uint16_t Padding;

  void SetFwdBck(uint32_t v) {
    FwdBckHi = v >> 8;
    ChildCountFwdBckLo &= 0xFFFFFF;
    ChildCountFwdBckLo |= v << 24;
  }

public:

  DxcHLSLNode() = default;

  DxcHLSLNode(D3D12_HLSL_NODE_TYPE NodeType, bool IsFwdDeclare,
              uint32_t LocalId, uint16_t AnnotationStart, uint32_t ChildCount,
              uint32_t ParentId, uint8_t AnnotationCount, uint16_t SemanticId)
      : LocalIdParentLo(LocalId | (ParentId << 24)), ParentHi(ParentId >> 8),
        Annotations(AnnotationCount), Type(NodeType),
        ChildCountFwdBckLo(ChildCount | (0xFFu << 24)), FwdBckHi(0xFFFF),
        AnnotationStart(AnnotationStart), SemanticId(SemanticId), Padding(0) {

    if (NodeType < D3D12_HLSL_NODE_TYPE_START ||
        NodeType > D3D12_HLSL_NODE_TYPE_END)
      throw std::invalid_argument("Invalid NodeType");

    if (LocalId >= ((1u << 24) - 1))
      throw std::invalid_argument("LocalId out of bounds");

    if (ParentId >= ((1u << 24) - 1))
      throw std::invalid_argument("ParentId out of bounds");

    if (ChildCount >= ((1u << 24) - 1))
      throw std::invalid_argument("ChildCount out of bounds");

    if (IsFwdDeclare) {

      if (AnnotationCount)
        throw std::invalid_argument("Fwd declares aren't allowed to have annotations");

      if (ChildCount)
        throw std::invalid_argument("Fwd declares aren't allowed to have children");

      Type |= 0x80;
    }
  }

  bool IsFwdDeclare() const { return Type >> 7; }

  // FwdBck is the only one that is allowed to break the DAG.
  // Example:
  // Node 0 (struct T;) <- IsFwdDeclare() and points to definition
  // Node 1 uses struct T;
  // Node 2 (struct T { float T; };) <- no fwd declare and points to fwd
  // declare.
  //
  // So the following are defined:
  // IsFwdDeclare() && IsFwdDefined(): GetFwdBck() should point to a valid node
  // that points back to the fwd declare.
  // !IsFwdDeclare() && IsFwdDefined(): GetFwdBck() should point to a valid fwd
  // declare that points back to it.
  // Backwards declare should point to a NodeId < self and Forwards to > self.
  // Forward declares aren't allowed to have any children.
  // If there's a name, they should match.
  // Must be same node type too.
  // Only allowed on functions, struct/union and enums.

  uint32_t GetFwdBck() const {
    return uint32_t(ChildCountFwdBckLo >> 24) | (uint32_t(FwdBckHi) << 8);
  }

  bool IsFwdBckDefined() const { return GetFwdBck() != ((1 << 24) - 1); }

  void ResolveFwdDeclare(uint32_t SelfId, DxcHLSLNode &Definition,
      uint32_t DefinitionId) {

    if (SelfId >= ((1u << 24) - 1))
      throw std::invalid_argument("SelfId out of bounds");

    if (DefinitionId >= ((1u << 24) - 1))
      throw std::invalid_argument("DefinitionId out of bounds");

    assert(DefinitionId != SelfId && "NodeId can't be definition id!");
    assert(IsFwdDeclare() &&
           "Can't run ResolveFwdDeclare on a node that's no fwd decl");

    assert(!Definition.IsFwdBckDefined() && !IsFwdBckDefined() &&
           "Fwd & backward declare must not be defined yet");

    SetFwdBck(DefinitionId);
    Definition.SetFwdBck(SelfId);
  }

  // For example if Enum, maps into Enums[LocalId]
  uint32_t GetLocalId() const { return LocalIdParentLo << 8 >> 8; }
  uint32_t GetAnnotationStart() const { return AnnotationStart; }

  uint32_t GetSemanticId() const {
    return SemanticId == uint16_t(-1) ? uint32_t(-1) : SemanticId;
  }

  D3D12_HLSL_NODE_TYPE GetNodeType() const {
    return D3D12_HLSL_NODE_TYPE(Type & 0x7F);
  }

  // Includes recursive children
  uint32_t GetChildCount() const { return ChildCountFwdBckLo << 8 >> 8; }

  uint32_t GetAnnotationCount() const { return Annotations; }

  uint32_t GetParentId() const {
    return uint32_t(LocalIdParentLo >> 24) | (uint32_t(ParentHi) << 8);
  }

  void IncreaseChildCount() {

    if (GetChildCount() >= ((1u << 24) - 1))
      throw std::invalid_argument("Child count out of bounds");

    ++ChildCountFwdBckLo;
  }

  bool operator==(const DxcHLSLNode &other) const {
    return LocalIdParentLo == other.LocalIdParentLo &&
           ParentHiAnnotationsType32 == other.ParentHiAnnotationsType32 &&
           ChildCountFwdBckLo == other.ChildCountFwdBckLo &&
           AnnotationStartFwdBckHi == other.AnnotationStartFwdBckHi &&
           SemanticId == other.SemanticId;
  }

};

struct DxcHLSLNodeSymbol {

  union {
    struct {
      uint32_t NameId; // Local name (not including parent's name)

      uint16_t FileSourceId;          //-1 == no file info
      uint16_t SourceLineCount;
    };
    uint64_t NameIdFileNameIdSourceLineCount;
  };

  union {
    struct {
      uint16_t SourceColumnStartLo;
      uint16_t SourceColumnEndLo;
      uint32_t ColumnHiSourceLinePad; // 6 : 6 : 20
    };
    uint64_t SourceColumnStartEndLo;
  };

  DxcHLSLNodeSymbol() = default;

  DxcHLSLNodeSymbol(uint32_t NameId, uint16_t FileSourceId,
                    uint16_t SourceLineCount, uint32_t SourceLineStart,
                    uint32_t SourceColumnStart, uint32_t SourceColumnEnd)
      : NameId(NameId), FileSourceId(FileSourceId),
        SourceLineCount(SourceLineCount),
        SourceColumnStartLo(uint16_t(SourceColumnStart)),
        SourceColumnEndLo(uint16_t(SourceColumnEnd)),
        ColumnHiSourceLinePad((SourceColumnStart >> 16) |
                              (SourceColumnEnd >> 16 << 6) |
                              (SourceLineStart << 12)) {

    if (SourceColumnStart >= (1u << 22))
      throw std::invalid_argument("SourceColumnStart out of bounds");

    if (SourceColumnEnd >= (1u << 22))
      throw std::invalid_argument("SourceColumnEnd out of bounds");

    if (SourceLineStart >= ((1u << 20) - 1))
      throw std::invalid_argument("SourceLineStart out of bounds");
  }

  uint32_t GetSourceLineStart() const {
    return uint32_t(ColumnHiSourceLinePad >> 12);
  }

  uint32_t GetSourceColumnStart() const {
    return SourceColumnStartLo | ((ColumnHiSourceLinePad & 0x3F) << 16);
  }

  uint32_t GetSourceColumnEnd() const {
    return SourceColumnEndLo | ((ColumnHiSourceLinePad & (0x3F << 6)) << 10);
  }

  bool operator==(const DxcHLSLNodeSymbol &other) const {
    return NameIdFileNameIdSourceLineCount ==
               other.NameIdFileNameIdSourceLineCount &&
           SourceColumnStartEndLo == other.SourceColumnStartEndLo;
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

/*
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
};*/

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

    if (NumParameters >= (1u << 30))
      throw std::invalid_argument("NumParameters out of bounds");
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

      uint32_t BindCount;
    };
    uint64_t TypeDimensionReturnTypeFlagsBindCount;
  };

  uint32_t Pad;
  uint32_t NodeId;

  union {
    struct {
      uint32_t ArrayId;  // Only if BindCount > 1 and the array is 2D+ (else -1)
      uint32_t BufferId; // If cbuffer or structured buffer
    };
    uint64_t ArrayIdBufferId;
  };

  DxcHLSLRegister() = default;
  DxcHLSLRegister(D3D_SHADER_INPUT_TYPE Type, uint32_t BindCount,
                  uint32_t uFlags, D3D_RESOURCE_RETURN_TYPE ReturnType,
                  D3D_SRV_DIMENSION Dimension, uint32_t NodeId,
                  uint32_t ArrayId, uint32_t BufferId)
      : Type(Type), BindCount(BindCount), uFlags(uFlags),
        ReturnType(ReturnType), Dimension(Dimension), NodeId(NodeId),
        ArrayId(ArrayId), BufferId(BufferId) {

    if (Type < D3D_SIT_CBUFFER || Type > D3D_SIT_UAV_FEEDBACKTEXTURE)
      throw std::invalid_argument("Invalid type");

    if (ReturnType < 0 ||
        ReturnType > D3D_RETURN_TYPE_CONTINUED)
      throw std::invalid_argument("Invalid return type");

    if (Dimension < D3D_SRV_DIMENSION_UNKNOWN ||
        Dimension > D3D_SRV_DIMENSION_BUFFEREX)
      throw std::invalid_argument("Invalid srv dimension");

    if (uFlags >> 8)
      throw std::invalid_argument("Invalid user flags");
  }

  bool operator==(const DxcHLSLRegister &other) const {
    return TypeDimensionReturnTypeFlagsBindCount ==
               other.TypeDimensionReturnTypeFlagsBindCount &&
           NodeId == other.NodeId && ArrayIdBufferId == other.ArrayIdBufferId;
  }
};

struct DxcHLSLArray {

  uint32_t ArrayElemStart;

  DxcHLSLArray() = default;
  DxcHLSLArray(uint32_t ArrayElem, uint32_t ArrayStart)
      : ArrayElemStart((ArrayElem << 26) | ArrayStart) {

    if (ArrayElem <= 1 || ArrayElem > 32)
      throw std::invalid_argument("ArrayElem out of bounds");

    if (ArrayStart >= (1u << 26))
      throw std::invalid_argument("ArrayStart out of bounds");
  }

  bool operator==(const DxcHLSLArray &Other) const {
    return Other.ArrayElemStart == ArrayElemStart;
  }

  uint32_t ArrayElem() const { return ArrayElemStart >> 26; }
  uint32_t ArrayStart() const { return ArrayElemStart << 6 >> 6; }
};

struct DxcHLSLType { // Almost maps to CShaderReflectionType and
                     // D3D12_SHADER_TYPE_DESC, but tightly packed and
                     // easily serializable

  uint32_t MemberData; // 24 : 8 (start, count)

  union {
    struct {
      uint8_t Class; // D3D_SHADER_VARIABLE_CLASS
      uint8_t Type;  // D3D_SHADER_VARIABLE_TYPE
      uint8_t Rows;
      uint8_t Columns;
    };
    uint32_t ClassTypeRowsColums;
  };

  uint32_t ElementsOrArrayId;
  uint32_t BaseClass; // -1 if none, otherwise a type index

  uint32_t InterfaceOffsetAndCount; // 24 : 8 (start, count)

  bool operator==(const DxcHLSLType &Other) const {
    return Other.MemberData == MemberData &&
           Other.ElementsOrArrayId == ElementsOrArrayId &&
           ClassTypeRowsColums == Other.ClassTypeRowsColums &&
           BaseClass == Other.BaseClass &&
           InterfaceOffsetAndCount == Other.InterfaceOffsetAndCount;
  }

  uint32_t GetMemberCount() const { return MemberData >> 24; }
  uint32_t GetMemberStart() const { return MemberData << 8 >> 8; }

  uint32_t GetInterfaceCount() const { return InterfaceOffsetAndCount >> 24; }
  uint32_t GetInterfaceStart() const {
    return InterfaceOffsetAndCount << 8 >> 8;
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
  DxcHLSLType(uint32_t BaseClass, uint32_t ElementsOrArrayId,
              D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type,
              uint8_t Rows, uint8_t Columns, uint32_t MembersCount,
              uint32_t MembersStart, uint32_t InterfaceOffset,
              uint32_t InterfaceCount)
      : MemberData(MembersStart | (MembersCount << 24)), Class(Class),
        Type(Type), Rows(Rows), Columns(Columns),
        ElementsOrArrayId(ElementsOrArrayId), BaseClass(BaseClass),
        InterfaceOffsetAndCount(InterfaceOffset | (InterfaceCount << 24)) {

    if (Class < D3D_SVC_SCALAR || Class > D3D_SVC_INTERFACE_POINTER)
      throw std::invalid_argument("Invalid class");

    if (Type < D3D_SVT_VOID || Type > D3D_SVT_UINT64)
      throw std::invalid_argument("Invalid type");

    if (MembersStart >= (1u << 24))
      throw std::invalid_argument("Member start out of bounds");

    if (InterfaceOffset >= (1u << 24))
      throw std::invalid_argument("Interface start out of bounds");

    if (MembersCount >= (1u << 8))
      throw std::invalid_argument("Member count out of bounds");

    if (InterfaceCount >= (1u << 8))
      throw std::invalid_argument("Interface count out of bounds");
  }
};

struct DxcHLSLBuffer { // Almost maps to CShaderReflectionConstantBuffer and
                       // D3D12_SHADER_BUFFER_DESC

  D3D_CBUFFER_TYPE Type;
  uint32_t NodeId;

  bool operator==(const DxcHLSLBuffer &other) const {
    return Type == other.Type && NodeId == other.NodeId;
  }
};

struct DxcHLSLAnnotation {

  uint32_t StringNonDebugAndIsBuiltin;

  DxcHLSLAnnotation() = default;

  DxcHLSLAnnotation(uint32_t StringNonDebug, bool IsBuiltin)
      : StringNonDebugAndIsBuiltin(StringNonDebug |
                                   (IsBuiltin ? (1u << 31) : 0)) {

    if (StringNonDebug >= (1u << 31))
      throw std::invalid_argument("String non debug out of bounds");
  }

  bool operator==(const DxcHLSLAnnotation &other) const {
    return StringNonDebugAndIsBuiltin == other.StringNonDebugAndIsBuiltin;
  }

  bool GetIsBuiltin() const { return StringNonDebugAndIsBuiltin >> 31; }

  uint32_t GetStringNonDebug() const {
    return StringNonDebugAndIsBuiltin << 1 >> 1;
  }
};

//Note: Regarding nodes, node 0 is the root node (global scope)
//      If a node is a fwd declare you should inspect the fwd node id.
//      If a node isn't a fwd declare but has a backward id, it should be ignored during traversal.
//      This node can be defined in a different namespace or type while it's declared elsewhere.
struct DxcHLSLReflectionData {

  D3D12_HLSL_REFLECTION_FEATURE Features{};

  std::vector<std::string> Strings;
  std::unordered_map<std::string, uint32_t> StringsToId;

  std::vector<std::string> StringsNonDebug;
  std::unordered_map<std::string, uint32_t> StringsToIdNonDebug;

  std::vector<uint32_t> Sources;
  std::unordered_map<std::string, uint16_t> StringToSourceId;

  std::vector<DxcHLSLNode> Nodes; // 0 = Root node (global scope)

  std::vector<DxcHLSLRegister> Registers;
  std::vector<DxcHLSLFunction> Functions;

  std::vector<DxcHLSLEnumDesc> Enums;
  std::vector<DxcHLSLEnumValue> EnumValues;

  // std::vector<DxcHLSLParameter> Parameters;
  std::vector<DxcHLSLAnnotation> Annotations;

  std::vector<DxcHLSLArray> Arrays;
  std::vector<uint32_t> ArraySizes;

  std::vector<uint32_t> MemberTypeIds;
  std::vector<uint32_t> TypeList;
  std::vector<DxcHLSLType> Types;
  std::vector<DxcHLSLBuffer> Buffers;

  // Can be stripped if !(D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)

  std::vector<DxcHLSLNodeSymbol> NodeSymbols;
  std::vector<uint32_t> MemberNameIds;
  std::vector<uint32_t> TypeNameIds;

  // Only generated if deserialized with MakeNameLookupTable or
  // GenerateNameLookupTable is called (and if symbols aren't stripped)

  std::unordered_map<std::string, uint32_t> FullyResolvedToNodeId;
  std::vector<std::string> NodeIdToFullyResolved;
  std::unordered_map<std::string, uint32_t> FullyResolvedToMemberId;

  void Dump(std::vector<std::byte> &Bytes) const;
  void Printf() const;
  void StripSymbols();
  bool GenerateNameLookupTable();

  DxcHLSLReflectionData() = default;
  DxcHLSLReflectionData(const std::vector<std::byte> &Bytes,
                    bool MakeNameLookupTable);

  static bool Initialize(clang::CompilerInstance &Compiler,
                         clang::TranslationUnitDecl &Ctx,
                         uint32_t AutoBindingSpace,
                         D3D12_HLSL_REFLECTION_FEATURE Features,
                         bool DefaultRowMaj, DxcHLSLReflectionData &Result);

  bool IsSameNonDebug(const DxcHLSLReflectionData &Other) const {
    return StringsNonDebug == Other.StringsNonDebug && Nodes == Other.Nodes &&
           Registers == Other.Registers && Functions == Other.Functions &&
           Enums == Other.Enums && EnumValues == Other.EnumValues &&
           Annotations == Other.Annotations && Arrays == Other.Arrays &&
           ArraySizes == Other.ArraySizes &&
           MemberTypeIds == Other.MemberTypeIds && TypeList == Other.TypeList &&
           Types == Other.Types && Buffers == Other.Buffers;
  }

  bool operator==(const DxcHLSLReflectionData &Other) const {
    return IsSameNonDebug(Other) && Strings == Other.Strings &&
           Sources == Other.Sources && NodeSymbols == Other.NodeSymbols &&
           MemberNameIds == Other.MemberNameIds &&
           TypeNameIds == Other.TypeNameIds;
  }
};

} // namespace hlsl

#pragma warning(default : 4201)
