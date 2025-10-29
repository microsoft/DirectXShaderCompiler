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
#include <stdexcept>

#include "d3d12shader.h"
#include "dxc/dxctools.h"

#pragma warning(disable : 4201)

namespace hlsl {

typedef const char *DxcReflectionError;
static constexpr const DxcReflectionError DxcReflectionSuccess = nullptr;

#ifndef NDEBUG
  #if defined(_MSC_VER)
    #define DXC_FUNC_NAME __FUNCTION__
  #elif defined(__clang__) || defined(__GNUC__)
    #define DXC_FUNC_NAME __PRETTY_FUNCTION__
  #else
    #define DXC_FUNC_NAME __func__
  #endif
  #define DXC_REFLECT_STRING(x) #x
  #define DXC_REFLECT_STRING2(x) DXC_REFLECT_STRING(x)
  #define DXC_REFLECT_ERR(x) (x " at " __FILE__ ":" DXC_REFLECT_STRING2(__LINE__) " (" DXC_FUNC_NAME ")")
#else
  #define DXC_REFLECT_ERR(x) x
#endif

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
  
  DxcHLSLNode(D3D12_HLSL_NODE_TYPE NodeType,
      bool IsFwdDeclare,
      uint32_t LocalId, uint16_t AnnotationStart, uint32_t ChildCount,
      uint32_t ParentId, uint8_t AnnotationCount, uint16_t SemanticId)
      : LocalIdParentLo(LocalId | (ParentId << 24)), ParentHi(ParentId >> 8),
      Annotations(AnnotationCount), Type(NodeType),
      ChildCountFwdBckLo(ChildCount | (0xFFu << 24)), FwdBckHi(0xFFFF),
      AnnotationStart(AnnotationStart), SemanticId(SemanticId), Padding(0) {

    if (IsFwdDeclare)
      Type |= 0x80;
  }

public:

  DxcHLSLNode() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLNode &OutNode, D3D12_HLSL_NODE_TYPE NodeType,
                                bool IsFwdDeclare,
              uint32_t LocalId, uint16_t AnnotationStart, uint32_t ChildCount,
              uint32_t ParentId, uint8_t AnnotationCount, uint16_t SemanticId) {

    if (NodeType < D3D12_HLSL_NODE_TYPE_START ||
        NodeType > D3D12_HLSL_NODE_TYPE_END)
      return DXC_REFLECT_ERR("Invalid NodeType");

    if (LocalId >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("LocalId out of bounds");

    if (ParentId >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("ParentId out of bounds");

    if (ChildCount >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("ChildCount out of bounds");

    if (IsFwdDeclare) {

      if (AnnotationCount)
        return DXC_REFLECT_ERR("Fwd declares aren't allowed to have annotations");

      if (ChildCount)
        return DXC_REFLECT_ERR("Fwd declares aren't allowed to have children");
    }

    OutNode = DxcHLSLNode(NodeType, IsFwdDeclare, LocalId, AnnotationStart,
                       ChildCount, ParentId, AnnotationCount, SemanticId);
    return DxcReflectionSuccess;
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

  [[nodiscard]] DxcReflectionError ResolveFwdDeclare(uint32_t SelfId,
                                                     DxcHLSLNode &Definition,
                                                     uint32_t DefinitionId) {

    if (SelfId >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("SelfId out of bounds");

    if (DefinitionId >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("DefinitionId out of bounds");

    assert(DefinitionId != SelfId && "NodeId can't be definition id!");
    assert(IsFwdDeclare() &&
           "Can't run ResolveFwdDeclare on a node that's no fwd decl");

    assert(!Definition.IsFwdBckDefined() && !IsFwdBckDefined() &&
           "Fwd & backward declare must not be defined yet");

    SetFwdBck(DefinitionId);
    Definition.SetFwdBck(SelfId);

    return DxcReflectionSuccess;
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

  [[nodiscard]] DxcReflectionError IncreaseChildCount() {

    if (GetChildCount() >= ((1u << 24) - 1))
      return DXC_REFLECT_ERR("Child count out of bounds");

    ++ChildCountFwdBckLo;
    return DxcReflectionSuccess;
  }

  bool operator==(const DxcHLSLNode &other) const {
    return LocalIdParentLo == other.LocalIdParentLo &&
           ParentHiAnnotationsType32 == other.ParentHiAnnotationsType32 &&
           ChildCountFwdBckLo == other.ChildCountFwdBckLo &&
           AnnotationStartFwdBckHi == other.AnnotationStartFwdBckHi &&
           SemanticId == other.SemanticId;
  }
};

class DxcHLSLNodeSymbol {

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
  
  DxcHLSLNodeSymbol(uint32_t NameId, uint16_t FileSourceId,
                    uint16_t SourceLineCount, uint32_t SourceLineStart,
                    uint32_t SourceColumnStart, uint32_t SourceColumnEnd)
      : NameId(NameId), FileSourceId(FileSourceId),
        SourceLineCount(SourceLineCount),
        SourceColumnStartLo(uint16_t(SourceColumnStart)),
        SourceColumnEndLo(uint16_t(SourceColumnEnd)),
        ColumnHiSourceLinePad((SourceColumnStart >> 16) |
                              (SourceColumnEnd >> 16 << 6) |
                              (SourceLineStart << 12)) {}

public:

  DxcHLSLNodeSymbol() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLNodeSymbol &Symbol, uint32_t NameId, uint16_t FileSourceId,
                    uint16_t SourceLineCount, uint32_t SourceLineStart,
                    uint32_t SourceColumnStart, uint32_t SourceColumnEnd) {

    if (SourceColumnStart >= (1u << 22))
      return DXC_REFLECT_ERR("SourceColumnStart out of bounds");

    if (SourceColumnEnd >= (1u << 22))
      return DXC_REFLECT_ERR("SourceColumnEnd out of bounds");

    if (SourceLineStart >= ((1u << 20) - 1))
      return DXC_REFLECT_ERR("SourceLineStart out of bounds");

    Symbol =
        DxcHLSLNodeSymbol(NameId, FileSourceId, SourceLineCount,
                          SourceLineStart, SourceColumnStart, SourceColumnEnd);
    return DxcReflectionSuccess;
  }

  
  uint32_t GetNameId() const { return NameId; }

  uint16_t GetFileSourceId() const { return FileSourceId; }
  uint16_t GetSourceLineCount() const { return SourceLineCount; }

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


struct DxcHLSLParameter { // Mirrors D3D12_PARAMETER_DESC without duplicating
                          // data (typeId holds some)

  uint32_t TypeId;
  uint32_t NodeId;

  uint8_t InterpolationMode; // D3D_INTERPOLATION_MODE
  uint8_t Flags;             // D3D_PARAMETER_FLAGS
  uint16_t Padding;

  bool operator==(const DxcHLSLParameter &other) const {
    return TypeId == other.TypeId && NodeId == other.NodeId &&
           InterpolationMode == other.InterpolationMode && Flags == other.Flags;
  }
};

// A statement is a for, while, if, switch. Basically every Stmt except do or scope.
// It can contain the following child nodes:
// - if HasConditionVar(): a variable in the condition
// - NodeCount children (If: children ex. else body, For: init children)
// - Rest of the body (If: else body, otherwise: normal body)
class DxcHLSLStatement {

  uint32_t NodeId;
  uint32_t NodeCount_HasConditionVar_HasElse;

  DxcHLSLStatement(uint32_t NodeId, uint32_t NodeCount, bool HasConditionVar,
                   bool IfAndHasElse)
      : NodeId(NodeId),
        NodeCount_HasConditionVar_HasElse(NodeCount |
                                          (HasConditionVar ? (1u << 30) : 0) |
                                          (IfAndHasElse ? (1u << 31) : 0)) {}

public:

  DxcHLSLStatement() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLStatement &Statement, uint32_t NodeId, uint32_t NodeCount,
             bool HasConditionVar, bool IfAndHasElse) {

    if (NodeCount >= (1u << 30))
      return DXC_REFLECT_ERR("NodeCount out of bounds");

    Statement =
        DxcHLSLStatement(NodeId, NodeCount, HasConditionVar, IfAndHasElse);
    return DxcReflectionSuccess;
  }

  uint32_t GetNodeId() const { return NodeId; }

  // Node count represents one of two things:
  // - If: The amount of nodes in the 'if' part of the branch (to be able to
  // find the else part)
  // - For: The amount of nodes in the initialize part of the for
  uint32_t GetNodeCount() const {
    return NodeCount_HasConditionVar_HasElse << 2 >> 2;
  }

  bool HasConditionVar() const {
    return (NodeCount_HasConditionVar_HasElse >> 30) & 1;
  }
  bool HasElse() const { return (NodeCount_HasConditionVar_HasElse >> 31) & 1; }

  bool operator==(const DxcHLSLStatement &Other) const {
    return NodeId == Other.NodeId &&
           NodeCount_HasConditionVar_HasElse ==
               Other.NodeCount_HasConditionVar_HasElse;
  }
};

class DxcHLSLFunction {

  uint32_t NodeId;
  uint32_t NumParametersHasReturnAndDefinition;

  DxcHLSLFunction(uint32_t NodeId, uint32_t NumParameters, bool HasReturn,
                  bool HasDefinition)
      : NodeId(NodeId),
        NumParametersHasReturnAndDefinition(NumParameters |
                                            (HasReturn ? (1u << 30) : 0) |
                                            (HasDefinition ? (1u << 31) : 0)) { }

public:

  DxcHLSLFunction() = default;

  [[nodiscard]] static DxcReflectionError Initialize(DxcHLSLFunction &Function, uint32_t NodeId,
                                uint32_t NumParameters, bool HasReturn,
                                bool HasDefinition) {

    if (NumParameters >= (1u << 30))
      return DXC_REFLECT_ERR("NumParameters out of bounds");

    Function = DxcHLSLFunction(NodeId, NumParameters, HasReturn, HasDefinition);
    return DxcReflectionSuccess;
  }

  uint32_t GetNodeId() const { return NodeId; }

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

class DxcHLSLRegister { // Almost maps to D3D12_SHADER_INPUT_BIND_DESC, minus
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

  DxcHLSLRegister(D3D_SHADER_INPUT_TYPE Type, uint32_t BindCount,
                  uint32_t uFlags, D3D_RESOURCE_RETURN_TYPE ReturnType,
                  D3D_SRV_DIMENSION Dimension, uint32_t NodeId,
                  uint32_t ArrayId, uint32_t BufferId)
      : Type(Type), BindCount(BindCount), uFlags(uFlags),
        ReturnType(ReturnType), Dimension(Dimension), NodeId(NodeId),
        ArrayId(ArrayId), BufferId(BufferId) {}

public:
  DxcHLSLRegister() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLRegister &Register, D3D_SHADER_INPUT_TYPE Type,
             uint32_t BindCount, uint32_t uFlags,
             D3D_RESOURCE_RETURN_TYPE ReturnType, D3D_SRV_DIMENSION Dimension,
             uint32_t NodeId, uint32_t ArrayId, uint32_t BufferId) {

    if (Type < D3D_SIT_CBUFFER || Type > D3D_SIT_UAV_FEEDBACKTEXTURE)
      return DXC_REFLECT_ERR("Invalid type");

    if (ReturnType < 0 || ReturnType > D3D_RETURN_TYPE_CONTINUED)
      return DXC_REFLECT_ERR("Invalid return type");

    if (Dimension < D3D_SRV_DIMENSION_UNKNOWN ||
        Dimension > D3D_SRV_DIMENSION_BUFFEREX)
      return DXC_REFLECT_ERR("Invalid srv dimension");

    if (uFlags >> 8)
      return DXC_REFLECT_ERR("Invalid user flags");

    Register = DxcHLSLRegister(Type, BindCount, uFlags, ReturnType, Dimension,
                               NodeId, ArrayId, BufferId);
    return DxcReflectionSuccess;
  }

  D3D_RESOURCE_RETURN_TYPE GetReturnType() const {
    return D3D_RESOURCE_RETURN_TYPE(ReturnType);
  }

  D3D_SRV_DIMENSION GetDimension() const {
    return D3D_SRV_DIMENSION(Dimension);
  }

  uint32_t GetFlags() const { return uFlags; }
  D3D_SHADER_INPUT_TYPE GetType() const { return D3D_SHADER_INPUT_TYPE(Type); }
  uint32_t GetBindCount() const { return BindCount; }
  uint32_t GetNodeId() const { return NodeId; }
  uint32_t GetArrayId() const { return ArrayId; }
  uint32_t GetBufferId() const { return BufferId; }

  bool operator==(const DxcHLSLRegister &other) const {
    return TypeDimensionReturnTypeFlagsBindCount ==
               other.TypeDimensionReturnTypeFlagsBindCount &&
           NodeId == other.NodeId && ArrayIdBufferId == other.ArrayIdBufferId;
  }
};

class DxcHLSLArray {

  uint32_t ArrayElemStart;

  DxcHLSLArray(uint32_t ArrayElem, uint32_t ArrayStart)
      : ArrayElemStart((ArrayElem << 26) | ArrayStart) {}

public:

  DxcHLSLArray() = default;

  [[nodiscard]] static DxcReflectionError Initialize(DxcHLSLArray &Arr, uint32_t ArrayElem, uint32_t ArrayStart) {

    if (ArrayElem <= 1 || ArrayElem > 32)
      return DXC_REFLECT_ERR("ArrayElem out of bounds");

    if (ArrayStart >= (1u << 26))
      return DXC_REFLECT_ERR("ArrayStart out of bounds");

    Arr = DxcHLSLArray(ArrayElem, ArrayStart);
    return DxcReflectionSuccess;
  }

  bool operator==(const DxcHLSLArray &Other) const {
    return Other.ArrayElemStart == ArrayElemStart;
  }

  uint32_t ArrayElem() const { return ArrayElemStart >> 26; }
  uint32_t ArrayStart() const { return ArrayElemStart << 6 >> 6; }
};

struct DxcHLSLArrayOrElements {

  uint32_t ElementsOrArrayId;

  DxcHLSLArrayOrElements() = default;
  DxcHLSLArrayOrElements(uint32_t arrayId, uint32_t arraySize)
      : ElementsOrArrayId(arrayId != (uint32_t)-1
                              ? ((1u << 31) | arrayId)
                              : (arraySize > 1 ? arraySize : 0)) {}

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

  bool operator==(const DxcHLSLArrayOrElements &Other) const {
    return Other.ElementsOrArrayId == ElementsOrArrayId;
  }
};

class DxcHLSLType { // Almost maps to CShaderReflectionType and
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

  uint32_t BaseClass;               // -1 if none, otherwise a type index
  uint32_t InterfaceOffsetAndCount; // 24 : 8 (start, count)

  DxcHLSLArrayOrElements UnderlyingArray;   //No sugar (e.g. F32x4a4 in using F32x4a4 = F32x4[4] becomes float4[4])

  DxcHLSLType(uint32_t BaseClass,
              DxcHLSLArrayOrElements ElementsOrArrayIdUnderlying,
              D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE Type,
              uint8_t Rows, uint8_t Columns, uint32_t MembersCount,
              uint32_t MembersStart, uint32_t InterfaceOffset,
              uint32_t InterfaceCount)
      : MemberData(MembersStart | (MembersCount << 24)), Class(Class),
        Type(Type), Rows(Rows), Columns(Columns),
        UnderlyingArray(ElementsOrArrayIdUnderlying), BaseClass(BaseClass),
        InterfaceOffsetAndCount(InterfaceOffset | (InterfaceCount << 24)) {}

public:

  bool operator==(const DxcHLSLType &Other) const {
    return Other.MemberData == MemberData &&
           Other.UnderlyingArray == UnderlyingArray &&
           ClassTypeRowsColums == Other.ClassTypeRowsColums &&
           BaseClass == Other.BaseClass &&
           InterfaceOffsetAndCount == Other.InterfaceOffsetAndCount;
  }

  D3D_SHADER_VARIABLE_CLASS GetClass() const {
    return D3D_SHADER_VARIABLE_CLASS(Class);
  }

  D3D_SHADER_VARIABLE_TYPE GetType() const {
    return D3D_SHADER_VARIABLE_TYPE(Type);
  }

  uint8_t GetRows() const { return Rows; }
  uint8_t GetColumns() const { return Columns; }

  uint32_t GetBaseClass() const { return BaseClass; }

  uint32_t GetMemberCount() const { return MemberData >> 24; }
  uint32_t GetMemberStart() const { return MemberData << 8 >> 8; }
  
  DxcHLSLArrayOrElements GetUnderlyingArray() const { return UnderlyingArray; }

  uint32_t GetInterfaceCount() const { return InterfaceOffsetAndCount >> 24; }
  uint32_t GetInterfaceStart() const {
    return InterfaceOffsetAndCount << 8 >> 8;
  }

  DxcHLSLType() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLType &Type, uint32_t BaseClass,
             DxcHLSLArrayOrElements ElementsOrArrayIdUnderlying,
             D3D_SHADER_VARIABLE_CLASS Class, D3D_SHADER_VARIABLE_TYPE VariableType,
             uint8_t Rows, uint8_t Columns, uint32_t MembersCount,
             uint32_t MembersStart, uint32_t InterfaceOffset,
             uint32_t InterfaceCount) {

    if (Class < D3D_SVC_SCALAR || Class > D3D_SVC_INTERFACE_POINTER)
      return DXC_REFLECT_ERR("Invalid class");

    if (VariableType < D3D_SVT_VOID || VariableType > D3D_SVT_UINT64)
      return DXC_REFLECT_ERR("Invalid type");

    if (MembersStart >= (1u << 24))
      return DXC_REFLECT_ERR("Member start out of bounds");

    if (InterfaceOffset >= (1u << 24))
      return DXC_REFLECT_ERR("Interface start out of bounds");

    if (MembersCount >= (1u << 8))
      return DXC_REFLECT_ERR("Member count out of bounds");

    if (InterfaceCount >= (1u << 8))
      return DXC_REFLECT_ERR("Interface count out of bounds");

    Type = DxcHLSLType(BaseClass, ElementsOrArrayIdUnderlying, Class,
                       VariableType, Rows, Columns, MembersCount, MembersStart,
                       InterfaceOffset, InterfaceCount);
    return DxcReflectionSuccess;
  }
};

struct DxcHLSLTypeSymbol {

  // Keep sugar (F32x4a4 = F32x4[4] stays F32x4a4, doesn't become float4[4] like
  // in underlying name + array)
  DxcHLSLArrayOrElements DisplayArray;

  // For example F32x4[4] stays F32x4 (array in DisplayArray)
  uint32_t DisplayNameId;

  // F32x4[4] would turn into float4 (array in UnderlyingArray)
  uint32_t UnderlyingNameId;

  bool operator==(const DxcHLSLTypeSymbol &Other) const {
    return Other.DisplayArray == DisplayArray &&
           DisplayNameId == Other.DisplayNameId &&
           UnderlyingNameId == Other.UnderlyingNameId;
  }

  DxcHLSLTypeSymbol() = default;
  DxcHLSLTypeSymbol(DxcHLSLArrayOrElements ElementsOrArrayIdDisplay,
                    uint32_t DisplayNameId, uint32_t UnderlyingNameId)
      : DisplayArray(ElementsOrArrayIdDisplay), DisplayNameId(DisplayNameId),
        UnderlyingNameId(UnderlyingNameId) {}
};

struct DxcHLSLBuffer { // Almost maps to CShaderReflectionConstantBuffer and
                       // D3D12_SHADER_BUFFER_DESC

  D3D_CBUFFER_TYPE Type;
  uint32_t NodeId;

  bool operator==(const DxcHLSLBuffer &other) const {
    return Type == other.Type && NodeId == other.NodeId;
  }
};

class DxcHLSLAnnotation {

  uint32_t StringNonDebugAndIsBuiltin;

  DxcHLSLAnnotation(uint32_t StringNonDebug, bool IsBuiltin)
      : StringNonDebugAndIsBuiltin(StringNonDebug |
                                   (IsBuiltin ? (1u << 31) : 0)) {}
public:

  DxcHLSLAnnotation() = default;

  [[nodiscard]] static DxcReflectionError
  Initialize(DxcHLSLAnnotation &Annotation, uint32_t StringNonDebug, bool IsBuiltin) {

    if (StringNonDebug >= (1u << 31))
      return DXC_REFLECT_ERR("String non debug out of bounds");

    Annotation = DxcHLSLAnnotation(StringNonDebug, IsBuiltin);
    return DxcReflectionSuccess;
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

  std::vector<DxcHLSLParameter> Parameters;
  std::vector<DxcHLSLAnnotation> Annotations;

  std::vector<DxcHLSLArray> Arrays;
  std::vector<uint32_t> ArraySizes;

  std::vector<uint32_t> MemberTypeIds;
  std::vector<uint32_t> TypeList;
  std::vector<DxcHLSLType> Types;
  std::vector<DxcHLSLBuffer> Buffers;

  std::vector<DxcHLSLStatement> Statements;

  // Can be stripped if !(D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)

  std::vector<DxcHLSLNodeSymbol> NodeSymbols;
  std::vector<uint32_t> MemberNameIds;
  std::vector<DxcHLSLTypeSymbol> TypeSymbols;

  // Only generated if deserialized with MakeNameLookupTable or
  // GenerateNameLookupTable is called (and if symbols aren't stripped)

  std::unordered_map<std::string, uint32_t> FullyResolvedToNodeId;
  std::vector<std::string> NodeIdToFullyResolved;
  std::unordered_map<std::string, uint32_t> FullyResolvedToMemberId;

  uint32_t RegisterString(const std::string &Name, bool IsNonDebug);
  [[nodiscard]] DxcReflectionError PushArray(uint32_t &ArrayId, uint32_t ArraySizeFlat,
                     const std::vector<uint32_t> &ArraySize);

  void RegisterTypeList(const std::vector<uint32_t> &TypeIds, uint32_t &Offset,
                        uint8_t &Len);

  static D3D_CBUFFER_TYPE GetBufferType(uint8_t Type);

  void Dump(std::vector<std::byte> &Bytes) const;
  void Printf() const;
  void StripSymbols();
  bool GenerateNameLookupTable();

  DxcHLSLReflectionData() = default;
  DxcHLSLReflectionData(const std::vector<std::byte> &Bytes,
                    bool MakeNameLookupTable);

  bool IsSameNonDebug(const DxcHLSLReflectionData &Other) const {
    return StringsNonDebug == Other.StringsNonDebug && Nodes == Other.Nodes &&
           Registers == Other.Registers && Functions == Other.Functions &&
           Enums == Other.Enums && EnumValues == Other.EnumValues &&
           Annotations == Other.Annotations && Arrays == Other.Arrays &&
           ArraySizes == Other.ArraySizes &&
           MemberTypeIds == Other.MemberTypeIds && TypeList == Other.TypeList &&
           Types == Other.Types && Buffers == Other.Buffers &&
           Parameters == Other.Parameters && Statements == Other.Statements;
  }

  bool operator==(const DxcHLSLReflectionData &Other) const {
    return IsSameNonDebug(Other) && Strings == Other.Strings &&
           Sources == Other.Sources && NodeSymbols == Other.NodeSymbols &&
           MemberNameIds == Other.MemberNameIds &&
           TypeSymbols == Other.TypeSymbols;
  }
};

} // namespace hlsl

#pragma warning(default : 4201)
