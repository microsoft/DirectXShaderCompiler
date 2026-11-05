///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflectionContainer.h                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <assert.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include "dxc/WinAdapter.h"
// need to disable this as it is voilated by this header
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
// Need to instruct non-windows compilers on what an interface is
#define interface struct
#endif

#include "d3d12shader.h"
#include "dxc/dxcreflect.h"

#pragma warning(disable : 4201)

namespace hlsl {

struct ReflectionError {

  const char *err;
  const char *func;

  uint32_t index; // For example which elementId made this error
  bool hasIndex;
  uint8_t pad[3];

  constexpr ReflectionError()
      : err(nullptr), func(nullptr), index(0), hasIndex(false), pad{0, 0, 0} {}

  constexpr ReflectionError(const char *err, const char *func)
      : err(err), func(func), index(0), hasIndex(false), pad{0, 0, 0} {}

  constexpr ReflectionError(const char *err, const char *func, uint32_t index)
      : err(err), func(func), index(index), hasIndex(true), pad{0, 0, 0} {}

  std::string toString() const {

    if (!err)
      return "";

    std::string res = err;

    if (hasIndex)
      res += " (at index " + std::to_string(index) + ")";

    if (func)
      res += " (" + std::string(func) + ")";

    return res;
  }

  operator bool() const { return err; }
};

static constexpr const ReflectionError ReflectionErrorSuccess = {};

#ifndef NDEBUG
#if defined(_MSC_VER)
#define HLSL_REFL_ERR_FUNC_NAME __FUNCTION__
#elif defined(__clang__) || defined(__GNUC__)
#define HLSL_REFL_ERR_FUNC_NAME __PRETTY_FUNCTION__
#else
#define HLSL_REFL_ERR_FUNC_NAME __func__
#endif
#define HLSL_REFL_ERR_STRING(x) #x
#define HLSL_REFL_ERR_STRING2(x) HLSL_REFL_ERR_STRING(x)
#define HLSL_REFL_ERR(x, ...)                                                  \
  ReflectionError(x " at " __FILE__ ":" HLSL_REFL_ERR_STRING2(__LINE__),       \
                  HLSL_REFL_ERR_FUNC_NAME, ##__VA_ARGS__)
#else
#define HLSL_REFL_ERR(x, ...) ReflectionError(x, nullptr, ##__VA_ARGS__)
#endif

class ReflectionNode {

  uint32_t LocalIdParentLo; // 24 : 8

  union {
    uint32_t ParentHiAnnotationsType32;
    struct {
      uint16_t ParentHi;
      uint8_t Annotations;
      uint8_t Type;
    };
  };

  uint32_t ChildCountFwdBckLo; // 24 : 8

  union {
    uint32_t AnnotationStartFwdBckHi;
    struct {
      uint16_t AnnotationStart;
      uint16_t FwdBckHi;
    };
  };

  union {
    uint32_t SemanticIdInterpolationMode;
    struct {
      uint16_t SemanticId;
      uint8_t InterpolationMode; // D3D_INTERPOLATION_MODE
      uint8_t Padding;
    };
  };

  void SetFwdBck(uint32_t v) {
    FwdBckHi = v >> 8;
    ChildCountFwdBckLo &= 0xFFFFFF;
    ChildCountFwdBckLo |= v << 24;
  }

  ReflectionNode(D3D12_HLSL_NODE_TYPE NodeType, bool IsFwdDeclare,
                 uint32_t LocalId, uint16_t AnnotationStart,
                 uint32_t ChildCount, uint32_t ParentId,
                 uint8_t AnnotationCount, uint16_t SemanticId,
                 D3D_INTERPOLATION_MODE InterpolationMode)
      : LocalIdParentLo(LocalId | (ParentId << 24)), ParentHi(ParentId >> 8),
        Annotations(AnnotationCount), Type(NodeType),
        ChildCountFwdBckLo(ChildCount | (0xFFu << 24)),
        AnnotationStart(AnnotationStart), FwdBckHi(0xFFFF),
        SemanticId(SemanticId), InterpolationMode(InterpolationMode),
        Padding(0) {

    if (IsFwdDeclare)
      Type |= 0x80;
  }

public:
  ReflectionNode() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionNode &OutNode, D3D12_HLSL_NODE_TYPE NodeType,
             bool IsFwdDeclare, uint32_t LocalId, uint16_t AnnotationStart,
             uint32_t ChildCount, uint32_t ParentId, uint8_t AnnotationCount,
             uint16_t SemanticId, D3D_INTERPOLATION_MODE InterpolationMode) {

    if (NodeType < D3D12_HLSL_NODE_TYPE_START ||
        NodeType > D3D12_HLSL_NODE_TYPE_END)
      return HLSL_REFL_ERR("Invalid NodeType");

    if (InterpolationMode < D3D_INTERPOLATION_UNDEFINED ||
        InterpolationMode > D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE)
      return HLSL_REFL_ERR("Invalid interpolation mode");

    if (LocalId >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("LocalId out of bounds");

    if (ParentId >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("ParentId out of bounds");

    if (ChildCount >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("ChildCount out of bounds");

    if (IsFwdDeclare && AnnotationCount)
      return HLSL_REFL_ERR("Fwd declares aren't allowed to have annotations");

    OutNode = ReflectionNode(NodeType, IsFwdDeclare, LocalId, AnnotationStart,
                             ChildCount, ParentId, AnnotationCount, SemanticId,
                             InterpolationMode);
    return ReflectionErrorSuccess;
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
  // Forward declares aren't allowed to have any children except functions,
  // which can have parameters only (inc. return).
  // If there's a name, they should match.
  // Must be same node type too.
  // Only allowed on functions, struct/union and enums.

  uint32_t GetFwdBck() const {
    return uint32_t(ChildCountFwdBckLo >> 24) | (uint32_t(FwdBckHi) << 8);
  }

  bool IsFwdBckDefined() const { return GetFwdBck() != ((1 << 24) - 1); }

  [[nodiscard]] ReflectionError ResolveFwdDeclare(uint32_t SelfId,
                                                  ReflectionNode &Definition,
                                                  uint32_t DefinitionId) {

    if (SelfId >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("SelfId out of bounds");

    if (DefinitionId >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("DefinitionId out of bounds");

    assert(DefinitionId != SelfId && "NodeId can't be definition id!");
    assert(IsFwdDeclare() &&
           "Can't run ResolveFwdDeclare on a node that's no fwd decl");

    assert(!Definition.IsFwdBckDefined() && !IsFwdBckDefined() &&
           "Fwd & backward declare must not be defined yet");

    SetFwdBck(DefinitionId);
    Definition.SetFwdBck(SelfId);

    return ReflectionErrorSuccess;
  }

  // For example if Enum, maps into Enums[LocalId]
  uint32_t GetLocalId() const { return LocalIdParentLo << 8 >> 8; }
  uint32_t GetAnnotationStart() const { return AnnotationStart; }

  uint32_t GetSemanticId() const {
    return SemanticId == uint16_t(-1) ? uint32_t(-1) : SemanticId;
  }

  D3D_INTERPOLATION_MODE GetInterpolationMode() const {
    return D3D_INTERPOLATION_MODE(InterpolationMode);
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

  [[nodiscard]] ReflectionError IncreaseChildCount() {

    if (GetChildCount() >= ((1u << 24) - 1))
      return HLSL_REFL_ERR("Child count out of bounds");

    ++ChildCountFwdBckLo;
    return ReflectionErrorSuccess;
  }

  bool operator==(const ReflectionNode &other) const {
    return LocalIdParentLo == other.LocalIdParentLo &&
           ParentHiAnnotationsType32 == other.ParentHiAnnotationsType32 &&
           ChildCountFwdBckLo == other.ChildCountFwdBckLo &&
           AnnotationStartFwdBckHi == other.AnnotationStartFwdBckHi &&
           SemanticIdInterpolationMode == other.SemanticIdInterpolationMode;
  }
};

class ReflectionNodeSymbol {

  union {
    struct {
      uint32_t NameId; // Local name (not including parent's name)

      uint16_t FileSourceId; //-1 == no file info
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

  ReflectionNodeSymbol(uint32_t NameId, uint16_t FileSourceId,
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
  ReflectionNodeSymbol() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionNodeSymbol &Symbol, uint32_t NameId,
             uint16_t FileSourceId, uint16_t SourceLineCount,
             uint32_t SourceLineStart, uint32_t SourceColumnStart,
             uint32_t SourceColumnEnd) {

    if (SourceColumnStart >= (1u << 22))
      return HLSL_REFL_ERR("SourceColumnStart out of bounds");

    if (SourceColumnEnd >= (1u << 22))
      return HLSL_REFL_ERR("SourceColumnEnd out of bounds");

    if (SourceLineStart >= ((1u << 20) - 1))
      return HLSL_REFL_ERR("SourceLineStart out of bounds");

    Symbol = ReflectionNodeSymbol(NameId, FileSourceId, SourceLineCount,
                                  SourceLineStart, SourceColumnStart,
                                  SourceColumnEnd);
    return ReflectionErrorSuccess;
  }

  uint32_t GetNameId() const { return NameId; }

  uint16_t GetFileSourceId() const { return FileSourceId; }
  bool HasFileSource() const { return FileSourceId != uint16_t(-1); }
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

  bool operator==(const ReflectionNodeSymbol &other) const {
    return NameIdFileNameIdSourceLineCount ==
               other.NameIdFileNameIdSourceLineCount &&
           SourceColumnStartEndLo == other.SourceColumnStartEndLo;
  }
};

struct ReflectionEnumeration {

  uint32_t NodeId;
  D3D12_HLSL_ENUM_TYPE Type;

  bool operator==(const ReflectionEnumeration &other) const {
    return NodeId == other.NodeId && Type == other.Type;
  }
};

struct ReflectionEnumValue {

  int64_t Value;
  uint32_t NodeId;

  bool operator==(const ReflectionEnumValue &other) const {
    return Value == other.Value && NodeId == other.NodeId;
  }
};

struct ReflectionFunctionParameter { // Mirrors D3D12_PARAMETER_DESC without
                                     // duplicating data (typeId holds some)

  uint32_t TypeId;
  uint32_t NodeId;

  uint32_t Flags; // D3D_PARAMETER_FLAGS

  bool operator==(const ReflectionFunctionParameter &other) const {
    return TypeId == other.TypeId && NodeId == other.NodeId &&
           Flags == other.Flags;
  }
};

// A statement is a for or a while statement.
// - if HasConditionVar(): a variable in the condition
// - NodeCount children (For: init children)
// - Rest of the body (body)
class ReflectionScopeStmt {

  uint32_t NodeId;
  uint32_t NodeCount_HasConditionVar_HasElse;

  ReflectionScopeStmt(uint32_t NodeId, uint32_t NodeCount, bool HasConditionVar,
                      bool IfAndHasElse)
      : NodeId(NodeId),
        NodeCount_HasConditionVar_HasElse(NodeCount |
                                          (HasConditionVar ? (1u << 30) : 0) |
                                          (IfAndHasElse ? (1u << 31) : 0)) {}

public:
  ReflectionScopeStmt() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionScopeStmt &Statement, uint32_t NodeId,
             uint32_t NodeCount, bool HasConditionVar, bool IfAndHasElse) {

    if (NodeCount >= (1u << 30))
      return HLSL_REFL_ERR("NodeCount out of bounds");

    Statement =
        ReflectionScopeStmt(NodeId, NodeCount, HasConditionVar, IfAndHasElse);
    return ReflectionErrorSuccess;
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

  bool operator==(const ReflectionScopeStmt &Other) const {
    return NodeId == Other.NodeId &&
           NodeCount_HasConditionVar_HasElse ==
               Other.NodeCount_HasConditionVar_HasElse;
  }
};

// An if/switch statement holds the following.
// - if HasConditionVar(): a variable in the switch condition (unused for if)
// - BranchCount (inc. default/else, implicit->childCount)
// Each branch then has a:
// - counter (or value for switch/case (non default))
// - if HasConditionVar(): a variable in the if
// - nodeCount (implicit) for the contents.
class ReflectionIfSwitchStmt {

  uint32_t NodeId;

  union {
    uint32_t ConditionVarElseOrDefault;
    struct {
      uint16_t Padding;
      bool ConditionVar;
      bool ElseOrDefault;
    };
  };

  ReflectionIfSwitchStmt(uint32_t NodeId, bool ConditionVar, bool ElseOrDefault)
      : NodeId(NodeId), Padding(0), ConditionVar(ConditionVar),
        ElseOrDefault(ElseOrDefault) {}

public:
  ReflectionIfSwitchStmt() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionIfSwitchStmt &IfSwitchStmt, uint32_t NodeId,
             bool HasConditionVar, bool HasElseOrDefault) {

    IfSwitchStmt =
        ReflectionIfSwitchStmt(NodeId, HasConditionVar, HasElseOrDefault);
    return ReflectionErrorSuccess;
  }

  uint32_t GetNodeId() const { return NodeId; }

  bool HasConditionVar() const { return ConditionVar; }
  bool HasElseOrDefault() const { return ElseOrDefault; }

  bool operator==(const ReflectionIfSwitchStmt &Other) const {
    return NodeId == Other.NodeId &&
           ConditionVarElseOrDefault == Other.ConditionVarElseOrDefault;
  }
};

class ReflectionBranchStmt {

  union {
    uint64_t NodeIdConditionVarType;
    struct {
      uint32_t NodeId;
      bool ConditionVar;
      bool ComplexCase;
      uint8_t ValueType; // D3D12_HLSL_ENUM_TYPE
      uint8_t Padding;
    };
  };

  uint64_t Value;

  ReflectionBranchStmt(uint32_t NodeId, bool ConditionVar, bool ComplexCase,
                       D3D12_HLSL_ENUM_TYPE ValueType, uint64_t Value)
      : NodeId(NodeId), ConditionVar(ConditionVar), ComplexCase(ComplexCase),
        ValueType(ValueType), Padding(0), Value(Value) {}

public:
  ReflectionBranchStmt() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionBranchStmt &BranchStmt, uint32_t NodeId,
             bool ConditionVar, bool ComplexCase,
             D3D12_HLSL_ENUM_TYPE ValueType, uint64_t Value) {

    if (ValueType < D3D12_HLSL_ENUM_TYPE_UINT ||
        ValueType > D3D12_HLSL_ENUM_TYPE_INT16_T)
      return HLSL_REFL_ERR("ValueType out of bounds");

    BranchStmt = ReflectionBranchStmt(NodeId, ConditionVar, ComplexCase,
                                      ValueType, Value);
    return ReflectionErrorSuccess;
  }

  // Only (16-bit, 32-bit or 64-bit)(signed, unsigned)
  D3D12_HLSL_ENUM_TYPE GetValueType() const {
    return D3D12_HLSL_ENUM_TYPE(ValueType);
  }

  uint32_t GetNodeId() const { return NodeId; }
  uint64_t GetValue() const { return Value; } // Manually cast this
  bool HasConditionVar() const { return ConditionVar; }
  bool IsComplexCase() const { return ComplexCase; }

  bool operator==(const ReflectionBranchStmt &Other) const {
    return NodeIdConditionVarType == Other.NodeIdConditionVarType &&
           Value == Other.Value;
  }
};

class ReflectionFunction {

  uint32_t NodeId;
  uint32_t NumParametersHasReturnAndDefinition;

  ReflectionFunction(uint32_t NodeId, uint32_t NumParameters, bool HasReturn,
                     bool HasDefinition)
      : NodeId(NodeId),
        NumParametersHasReturnAndDefinition(NumParameters |
                                            (HasReturn ? (1u << 30) : 0) |
                                            (HasDefinition ? (1u << 31) : 0)) {}

public:
  ReflectionFunction() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionFunction &Function, uint32_t NodeId,
             uint32_t NumParameters, bool HasReturn, bool HasDefinition) {

    if (NumParameters >= (1u << 30))
      return HLSL_REFL_ERR("NumParameters out of bounds");

    Function =
        ReflectionFunction(NodeId, NumParameters, HasReturn, HasDefinition);
    return ReflectionErrorSuccess;
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

  bool operator==(const ReflectionFunction &other) const {
    return NodeId == other.NodeId &&
           NumParametersHasReturnAndDefinition ==
               other.NumParametersHasReturnAndDefinition;
  }
};

class ReflectionShaderResource { // Almost maps to D3D12_SHADER_INPUT_BIND_DESC,
                                 // minus
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

  ReflectionShaderResource(D3D_SHADER_INPUT_TYPE Type, uint32_t BindCount,
                           uint32_t uFlags, D3D_RESOURCE_RETURN_TYPE ReturnType,
                           D3D_SRV_DIMENSION Dimension, uint32_t NodeId,
                           uint32_t ArrayId, uint32_t BufferId)
      : Type(Type), Dimension(Dimension), ReturnType(ReturnType),
        uFlags(uFlags), BindCount(BindCount), Pad(0), NodeId(NodeId),
        ArrayId(ArrayId), BufferId(BufferId) {}

public:
  ReflectionShaderResource() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionShaderResource &Register, D3D_SHADER_INPUT_TYPE Type,
             uint32_t BindCount, uint32_t uFlags,
             D3D_RESOURCE_RETURN_TYPE ReturnType, D3D_SRV_DIMENSION Dimension,
             uint32_t NodeId, uint32_t ArrayId, uint32_t BufferId) {

    if (Type < D3D_SIT_CBUFFER || Type > D3D_SIT_UAV_FEEDBACKTEXTURE)
      return HLSL_REFL_ERR("Invalid type");

    if (ReturnType < 0 || ReturnType > D3D_RETURN_TYPE_CONTINUED)
      return HLSL_REFL_ERR("Invalid return type");

    if (Dimension < D3D_SRV_DIMENSION_UNKNOWN ||
        Dimension > D3D_SRV_DIMENSION_BUFFEREX)
      return HLSL_REFL_ERR("Invalid srv dimension");

    if (uFlags >> 8)
      return HLSL_REFL_ERR("Invalid user flags");

    Register = ReflectionShaderResource(Type, BindCount, uFlags, ReturnType,
                                        Dimension, NodeId, ArrayId, BufferId);
    return ReflectionErrorSuccess;
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

  bool operator==(const ReflectionShaderResource &other) const {
    return TypeDimensionReturnTypeFlagsBindCount ==
               other.TypeDimensionReturnTypeFlagsBindCount &&
           NodeId == other.NodeId && ArrayIdBufferId == other.ArrayIdBufferId;
  }
};

class ReflectionArray {

  uint32_t ArrayElemStart;

  ReflectionArray(uint32_t ArrayElem, uint32_t ArrayStart)
      : ArrayElemStart((ArrayElem << 26) | ArrayStart) {}

public:
  ReflectionArray() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionArray &Arr, uint32_t ArrayElem, uint32_t ArrayStart) {

    if (ArrayElem <= 1 || ArrayElem > 32)
      return HLSL_REFL_ERR("ArrayElem out of bounds");

    if (ArrayStart >= (1u << 26))
      return HLSL_REFL_ERR("ArrayStart out of bounds");

    Arr = ReflectionArray(ArrayElem, ArrayStart);
    return ReflectionErrorSuccess;
  }

  bool operator==(const ReflectionArray &Other) const {
    return Other.ArrayElemStart == ArrayElemStart;
  }

  uint32_t ArrayElem() const { return ArrayElemStart >> 26; }
  uint32_t ArrayStart() const { return ArrayElemStart << 6 >> 6; }
};

struct ReflectionArrayOrElements {

  uint32_t ElementsOrArrayId;

  ReflectionArrayOrElements() = default;
  ReflectionArrayOrElements(uint32_t arrayId, uint32_t arraySize)
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

  bool operator==(const ReflectionArrayOrElements &Other) const {
    return Other.ElementsOrArrayId == ElementsOrArrayId;
  }
};

class ReflectionVariableType { // Almost maps to CShaderReflectionType and
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

  ReflectionArrayOrElements
      UnderlyingArray; // No sugar (e.g. F32x4a4 in using F32x4a4 = F32x4[4]
                       // becomes float4[4])

  ReflectionVariableType(uint32_t BaseClass,
                         ReflectionArrayOrElements ElementsOrArrayIdUnderlying,
                         D3D_SHADER_VARIABLE_CLASS Class,
                         D3D_SHADER_VARIABLE_TYPE Type, uint8_t Rows,
                         uint8_t Columns, uint32_t MembersCount,
                         uint32_t MembersStart, uint32_t InterfaceOffset,
                         uint32_t InterfaceCount)
      : MemberData(MembersStart | (MembersCount << 24)), Class(Class),
        Type(Type), Rows(Rows), Columns(Columns), BaseClass(BaseClass),
        InterfaceOffsetAndCount(InterfaceOffset | (InterfaceCount << 24)),
        UnderlyingArray(ElementsOrArrayIdUnderlying) {}

public:
  bool operator==(const ReflectionVariableType &Other) const {
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

  ReflectionArrayOrElements GetUnderlyingArray() const {
    return UnderlyingArray;
  }

  uint32_t GetInterfaceCount() const { return InterfaceOffsetAndCount >> 24; }
  uint32_t GetInterfaceStart() const {
    return InterfaceOffsetAndCount << 8 >> 8;
  }

  ReflectionVariableType() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionVariableType &Type, uint32_t BaseClass,
             ReflectionArrayOrElements ElementsOrArrayIdUnderlying,
             D3D_SHADER_VARIABLE_CLASS Class,
             D3D_SHADER_VARIABLE_TYPE VariableType, uint8_t Rows,
             uint8_t Columns, uint32_t MembersCount, uint32_t MembersStart,
             uint32_t InterfaceOffset, uint32_t InterfaceCount) {

    if (Class < D3D_SVC_SCALAR || Class > D3D_SVC_INTERFACE_POINTER)
      return HLSL_REFL_ERR("Invalid class");

    if (VariableType < D3D_SVT_VOID || VariableType > D3D_SVT_UINT64)
      return HLSL_REFL_ERR("Invalid type");

    if (MembersStart >= (1u << 24))
      return HLSL_REFL_ERR("Member start out of bounds");

    if (InterfaceOffset >= (1u << 24))
      return HLSL_REFL_ERR("Interface start out of bounds");

    if (MembersCount >= (1u << 8))
      return HLSL_REFL_ERR("Member count out of bounds");

    if (InterfaceCount >= (1u << 8))
      return HLSL_REFL_ERR("Interface count out of bounds");

    Type = ReflectionVariableType(
        BaseClass, ElementsOrArrayIdUnderlying, Class, VariableType, Rows,
        Columns, MembersCount, MembersStart, InterfaceOffset, InterfaceCount);
    return ReflectionErrorSuccess;
  }
};

struct ReflectionVariableTypeSymbol {

  // Keep sugar (F32x4a4 = F32x4[4] stays F32x4a4, doesn't become float4[4] like
  // in underlying name + array)
  ReflectionArrayOrElements DisplayArray;

  // For example F32x4[4] stays F32x4 (array in DisplayArray)
  uint32_t DisplayNameId;

  // F32x4[4] would turn into float4 (array in UnderlyingArray)
  uint32_t UnderlyingNameId;

  bool operator==(const ReflectionVariableTypeSymbol &Other) const {
    return Other.DisplayArray == DisplayArray &&
           DisplayNameId == Other.DisplayNameId &&
           UnderlyingNameId == Other.UnderlyingNameId;
  }

  ReflectionVariableTypeSymbol() = default;
  ReflectionVariableTypeSymbol(
      ReflectionArrayOrElements ElementsOrArrayIdDisplay,
      uint32_t DisplayNameId, uint32_t UnderlyingNameId)
      : DisplayArray(ElementsOrArrayIdDisplay), DisplayNameId(DisplayNameId),
        UnderlyingNameId(UnderlyingNameId) {}
};

struct ReflectionShaderBuffer { // Almost maps to
                                // CShaderReflectionConstantBuffer and
                                // D3D12_SHADER_BUFFER_DESC

  D3D_CBUFFER_TYPE Type;
  uint32_t NodeId;

  bool operator==(const ReflectionShaderBuffer &other) const {
    return Type == other.Type && NodeId == other.NodeId;
  }
};

class ReflectionAnnotation {

  uint32_t StringNonDebugAndIsBuiltin;

  ReflectionAnnotation(uint32_t StringNonDebug, bool IsBuiltin)
      : StringNonDebugAndIsBuiltin(StringNonDebug |
                                   (IsBuiltin ? (1u << 31) : 0)) {}

public:
  ReflectionAnnotation() = default;

  [[nodiscard]] static ReflectionError
  Initialize(ReflectionAnnotation &Annotation, uint32_t StringNonDebug,
             bool IsBuiltin) {

    if (StringNonDebug >= (1u << 31))
      return HLSL_REFL_ERR("String non debug out of bounds");

    Annotation = ReflectionAnnotation(StringNonDebug, IsBuiltin);
    return ReflectionErrorSuccess;
  }

  bool operator==(const ReflectionAnnotation &other) const {
    return StringNonDebugAndIsBuiltin == other.StringNonDebugAndIsBuiltin;
  }

  bool GetIsBuiltin() const { return StringNonDebugAndIsBuiltin >> 31; }

  uint32_t GetStringNonDebug() const {
    return StringNonDebugAndIsBuiltin << 1 >> 1;
  }
};

// Note: Regarding nodes, node 0 is the root node (global scope)
//       If a node is a fwd declare you should inspect the fwd node id.
//       If a node isn't a fwd declare but has a backward id, the node should be
//       ignored during traversal. (That node can be defined in a different
//       namespace or type while it's declared elsewhere).
struct ReflectionData {

  D3D12_HLSL_REFLECTION_FEATURE Features{};

  std::vector<std::string> Strings;
  std::unordered_map<std::string, uint32_t> StringsToId;

  std::vector<std::string> StringsNonDebug;
  std::unordered_map<std::string, uint32_t> StringsToIdNonDebug;

  std::vector<uint32_t> Sources;
  std::unordered_map<std::string, uint16_t> StringToSourceId;

  std::vector<ReflectionNode> Nodes; // 0 = Root node (global scope)

  std::vector<ReflectionShaderResource> Registers;
  std::vector<ReflectionFunction> Functions;

  std::vector<ReflectionEnumeration> Enums;
  std::vector<ReflectionEnumValue> EnumValues;

  std::vector<ReflectionFunctionParameter> Parameters;
  std::vector<ReflectionAnnotation> Annotations;

  std::vector<ReflectionArray> Arrays;
  std::vector<uint32_t> ArraySizes;

  std::vector<uint32_t> MemberTypeIds;
  std::vector<uint32_t> TypeList;
  std::vector<ReflectionVariableType> Types;
  std::vector<ReflectionShaderBuffer> Buffers;

  std::vector<ReflectionScopeStmt> Statements;
  std::vector<ReflectionIfSwitchStmt> IfSwitchStatements;
  std::vector<ReflectionBranchStmt> BranchStatements;

  // Can be stripped if !(D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)

  std::vector<ReflectionNodeSymbol> NodeSymbols;
  std::vector<uint32_t> MemberNameIds;
  std::vector<ReflectionVariableTypeSymbol> TypeSymbols;

  // Only generated if deserialized with MakeNameLookupTable or
  // GenerateNameLookupTable is called (and if symbols aren't stripped)

  std::unordered_map<std::string, uint32_t> FullyResolvedToNodeId;
  std::vector<std::string> NodeIdToFullyResolved;
  std::unordered_map<std::string, uint32_t> FullyResolvedToMemberId;

  [[nodiscard]] ReflectionError
  RegisterString(uint32_t &StringId, const std::string &Name, bool IsNonDebug);

  [[nodiscard]] ReflectionError
  PushArray(uint32_t &ArrayId, uint32_t ArraySizeFlat,
            const std::vector<uint32_t> &ArraySize);

  [[nodiscard]] ReflectionError
  RegisterTypeList(const std::vector<uint32_t> &TypeIds, uint32_t &Offset,
                   uint8_t &Len);

  static D3D_CBUFFER_TYPE GetBufferType(uint8_t Type);

  void Dump(std::vector<std::byte> &Bytes) const;
  std::string ToJson(bool HideFileInfo = false,
                     bool IsHumanFriendly = true) const;

  void StripSymbols();
  bool GenerateNameLookupTable();

  ReflectionData() = default;
  [[nodiscard]] ReflectionError Deserialize(const std::vector<std::byte> &Bytes,
                                            bool MakeNameLookupTable);

  bool IsSameNonDebug(const ReflectionData &Other) const {
    return StringsNonDebug == Other.StringsNonDebug && Nodes == Other.Nodes &&
           Registers == Other.Registers && Functions == Other.Functions &&
           Enums == Other.Enums && EnumValues == Other.EnumValues &&
           Annotations == Other.Annotations && Arrays == Other.Arrays &&
           ArraySizes == Other.ArraySizes &&
           MemberTypeIds == Other.MemberTypeIds && TypeList == Other.TypeList &&
           Types == Other.Types && Buffers == Other.Buffers &&
           Parameters == Other.Parameters && Statements == Other.Statements &&
           IfSwitchStatements == Other.IfSwitchStatements &&
           BranchStatements == Other.BranchStatements;
  }

  bool operator==(const ReflectionData &Other) const {
    return IsSameNonDebug(Other) && Strings == Other.Strings &&
           Sources == Other.Sources && NodeSymbols == Other.NodeSymbols &&
           MemberNameIds == Other.MemberNameIds &&
           TypeSymbols == Other.TypeSymbols;
  }
};

} // namespace hlsl

#pragma warning(default : 4201)
