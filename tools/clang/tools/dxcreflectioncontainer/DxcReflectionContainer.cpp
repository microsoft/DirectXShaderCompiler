///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflectionContainer.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxcReflection/DxcReflectionContainer.h"
#include <inttypes.h>
#include <stdexcept>

#undef min
#undef max

namespace hlsl {

[[nodiscard]] ReflectionError
ReflectionData::RegisterString(uint32_t &StringId, const std::string &Name,
                               bool IsNonDebug) {

  if (Name.size() >= 32768)
    return HLSL_REFL_ERR("Strings are limited to 32767");

  if (IsNonDebug) {

    if (StringsNonDebug.size() >= uint32_t(-1))
      return HLSL_REFL_ERR("Strings overflow");

    auto it = StringsToIdNonDebug.find(Name);

    if (it != StringsToIdNonDebug.end()) {
      StringId = it->second;
      return ReflectionErrorSuccess;
    }

    uint32_t stringId = uint32_t(StringsNonDebug.size());

    StringsNonDebug.push_back(Name);
    StringsToIdNonDebug[Name] = stringId;
    StringId = stringId;
    return ReflectionErrorSuccess;
  }

  if (Strings.size() >= uint32_t(-1))
    return HLSL_REFL_ERR("Strings overflow");

  auto it = StringsToId.find(Name);

  if (it != StringsToId.end()) {
    StringId = it->second;
    return ReflectionErrorSuccess;
  }

  uint32_t stringId = uint32_t(Strings.size());

  Strings.push_back(Name);
  StringsToId[Name] = stringId;
  StringId = stringId;
  return ReflectionErrorSuccess;
}

[[nodiscard]] ReflectionError
ReflectionData::PushArray(uint32_t &ArrayId, uint32_t ArraySizeFlat,
                          const std::vector<uint32_t> &ArraySize) {

  if (ArraySizeFlat <= 1 || ArraySize.size() <= 1) {
    ArrayId = uint32_t(-1);
    return ReflectionErrorSuccess;
  }

  if (Arrays.size() >= uint32_t((1u << 31) - 1))
    return HLSL_REFL_ERR("Arrays would overflow");

  uint32_t arrayId = uint32_t(Arrays.size());

  uint32_t arrayCountStart = uint32_t(ArraySizes.size());
  uint32_t numArrayElements = std::min(size_t(32), ArraySize.size());

  if (ArraySizes.size() + numArrayElements >= ((1u << 26) - 1))
    return HLSL_REFL_ERR("Array elements would overflow");

  for (uint32_t i = 0; i < ArraySize.size() && i < 8; ++i) {

    uint32_t arraySize = ArraySize[i];

    // Flatten rest of array to at least keep consistent array elements
    if (i == 31)
      for (uint32_t j = i + 1; j < ArraySize.size(); ++j)
        arraySize *= ArraySize[j];

    ArraySizes.push_back(arraySize);
  }

  ReflectionArray arr;

  if (ReflectionError err =
          ReflectionArray::Initialize(arr, numArrayElements, arrayCountStart))
    return err;

  for (uint32_t i = 0; i < Arrays.size(); ++i)
    if (Arrays[i] == arr) {
      ArrayId = i;
      return ReflectionErrorSuccess;
    }

  Arrays.push_back(arr);
  ArrayId = arrayId;
  return ReflectionErrorSuccess;
}

[[nodiscard]] ReflectionError
ReflectionData::RegisterTypeList(const std::vector<uint32_t> &TypeIds,
                                 uint32_t &Offset, uint8_t &Len) {

  if (TypeIds.empty())
    return ReflectionErrorSuccess;

  if (TypeIds.size() >= uint8_t(-1))
    return HLSL_REFL_ERR("Only allowing 256 types in a type list");

  uint32_t i = 0;
  uint32_t j = uint32_t(TypeList.size());
  uint32_t k = 0;

  Offset = 0;

  for (; i < j; ++i) {

    if (k == TypeIds.size())
      break;

    if (TypeIds[k] != TypeList[i]) {

      if (k)
        i = Offset;

      k = 0;
      break;
    }

    if (!k)
      Offset = i;

    ++k;
  }

  if (k != TypeIds.size()) {

    uint32_t oldSiz = uint32_t(TypeList.size());

    if (oldSiz + TypeIds.size() >= (1u << 24))
      return HLSL_REFL_ERR("Only allowing 16Mi total interfaces");

    TypeList.resize(oldSiz + TypeIds.size());

    std::memcpy(TypeList.data() + oldSiz, TypeIds.data(),
                TypeIds.size() * sizeof(uint32_t));

    Offset = oldSiz;
  }

  Len = uint8_t(TypeIds.size());
  return ReflectionErrorSuccess;
}

struct HLSLReflectionDataHeader {

  uint32_t MagicNumber;
  uint16_t Version;
  uint16_t Sources;

  D3D12_HLSL_REFLECTION_FEATURE Features;
  uint32_t StringsNonDebug;

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

  uint32_t TypeListCount;
  uint32_t Parameters;

  uint32_t Statements;
  uint32_t IfSwitchStatements;

  uint32_t BranchStatements;
  uint32_t Pad;
};

template <typename T>
T &UnsafeCast(std::vector<std::byte> &Bytes, uint64_t Offset) {
  return *(T *)(Bytes.data() + Offset);
}

template <typename T>
const T &UnsafeCast(const std::vector<std::byte> &Bytes, uint64_t Offset) {
  return *(const T *)(Bytes.data() + Offset);
}

template <typename T> void SkipPadding(uint64_t &Offset) {
  Offset = (Offset + alignof(T) - 1) / alignof(T) * alignof(T);
}

template <typename T> void Skip(uint64_t &Offset, const std::vector<T> &Vec) {
  Offset += Vec.size() * sizeof(T);
}

template <typename T>
void Advance(uint64_t &Offset, const std::vector<T> &Vec) {
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

template <typename T, typename T2, typename... args>
void Advance(uint64_t &Offset, const std::vector<T> &Vec,
             const std::vector<T2> &Vec2, args... arg) {
  Advance(Offset, Vec);
  Advance(Offset, Vec2, arg...);
}

template <typename T>
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

template <typename T, typename T2, typename... args>
void Append(std::vector<std::byte> &Bytes, uint64_t &Offset,
            const std::vector<T> &Vec, const std::vector<T2> &Vec2,
            args... arg) {
  Append(Bytes, Offset, Vec);
  Append(Bytes, Offset, Vec2, arg...);
}

template <typename T, typename = std::enable_if_t<std::is_pod_v<T>>>
[[nodiscard]] ReflectionError Consume(const std::vector<std::byte> &Bytes,
                                      uint64_t &Offset, T &t) {

  static_assert(std::is_pod_v<T>, "Consume only works on POD types");

  SkipPadding<T>(Offset);

  if (Offset + sizeof(T) > Bytes.size())
    return HLSL_REFL_ERR("Couldn't consume; out of bounds!");

  std::memcpy(&t, &UnsafeCast<uint8_t>(Bytes, Offset), sizeof(T));
  Offset += sizeof(T);

  return ReflectionErrorSuccess;
}

template <typename T>
[[nodiscard]] ReflectionError Consume(const std::vector<std::byte> &Bytes,
                                      uint64_t &Offset, T *target,
                                      uint64_t Len) {

  static_assert(std::is_pod_v<T>, "Consume only works on POD types");

  SkipPadding<T>(Offset);

  if (Offset + sizeof(T) * Len > Bytes.size())
    return HLSL_REFL_ERR("Couldn't consume; out of bounds!");

  std::memcpy(target, &UnsafeCast<uint8_t>(Bytes, Offset), sizeof(T) * Len);
  Offset += sizeof(T) * Len;

  return ReflectionErrorSuccess;
}

template <typename T>
[[nodiscard]] ReflectionError Consume(const std::vector<std::byte> &Bytes,
                                      uint64_t &Offset, std::vector<T> &Vec,
                                      uint64_t Len) {
  Vec.resize(Len);
  return Consume(Bytes, Offset, Vec.data(), Len);
}

template <>
[[nodiscard]] ReflectionError
Consume<std::string>(const std::vector<std::byte> &Bytes, uint64_t &Offset,
                     std::vector<std::string> &Vec, uint64_t Len) {
  Vec.resize(Len);

  for (uint64_t i = 0; i < Len; ++i) {

    if (Offset >= Bytes.size())
      return HLSL_REFL_ERR("Couldn't consume string len; out of bounds!");

    uint16_t ourLen = uint8_t(Bytes.at(Offset++));

    if (ourLen >> 7) {

      if (Offset >= Bytes.size())
        return HLSL_REFL_ERR("Couldn't consume string len; out of bounds!");

      ourLen &= ~(1 << 7);
      ourLen |= uint16_t(Bytes.at(Offset++)) << 7;
    }

    if (Offset + ourLen > Bytes.size())
      return HLSL_REFL_ERR("Couldn't consume string len; out of bounds!");

    Vec[i].resize(ourLen);
    std::memcpy(Vec[i].data(), Bytes.data() + Offset, ourLen);
    Offset += ourLen;
  }

  return ReflectionErrorSuccess;
}

template <typename T, typename T2, typename... args>
[[nodiscard]] ReflectionError Consume(const std::vector<std::byte> &Bytes,
                                      uint64_t &Offset, std::vector<T> &Vec,
                                      uint64_t Len, std::vector<T2> &Vec2,
                                      uint64_t Len2, args &...arg) {

  if (ReflectionError err = Consume(Bytes, Offset, Vec, Len))
    return err;

  if (ReflectionError err = Consume(Bytes, Offset, Vec2, Len2, arg...))
    return err;

  return ReflectionErrorSuccess;
}

static constexpr uint32_t ReflectionDataMagic = DXC_FOURCC('H', 'L', 'R', 'D');
static constexpr uint16_t ReflectionDataVersion = 0;

void ReflectionData::StripSymbols() {
  Strings.clear();
  StringsToId.clear();
  Sources.clear();
  StringToSourceId.clear();
  FullyResolvedToNodeId.clear();
  NodeIdToFullyResolved.clear();
  FullyResolvedToMemberId.clear();
  NodeSymbols.clear();
  TypeSymbols.clear();
  MemberNameIds.clear();
  Features &= ~D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;
}

void RecurseNameGenerationType(ReflectionData &Refl, uint32_t TypeId,
                               uint32_t LocalId, const std::string &Parent) {

  const ReflectionVariableType &type = Refl.Types[TypeId];

  if (type.GetClass() == D3D_SVC_STRUCT)
    for (uint32_t i = 0; i < type.GetMemberCount(); ++i) {

      uint32_t memberId = i + type.GetMemberStart();
      std::string memberName =
          Parent + "." + Refl.Strings[Refl.MemberNameIds[memberId]];

      Refl.FullyResolvedToMemberId[memberName] = memberId;

      RecurseNameGenerationType(Refl, Refl.MemberTypeIds[memberId], i,
                                memberName);
    }
}

uint32_t RecurseNameGeneration(ReflectionData &Refl, uint32_t NodeId,
                               uint32_t LocalId, const std::string &Parent,
                               bool IsDot) {

  ReflectionNode node = Refl.Nodes[NodeId];

  if (node.IsFwdDeclare() && node.IsFwdBckDefined()) {
    NodeId = node.GetFwdBck();
    node = Refl.Nodes[NodeId];
  }

  std::string self = Refl.Strings[Refl.NodeSymbols[NodeId].GetNameId()];

  if (self.empty() && NodeId)
    self = std::to_string(LocalId);

  self = Parent.empty() ? self : Parent + (IsDot ? "." : "::") + self;
  Refl.FullyResolvedToNodeId[self] = NodeId;
  Refl.NodeIdToFullyResolved[NodeId] = self;

  bool isDotChild = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_REGISTER;

  bool isVar = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_VARIABLE ||
               node.GetNodeType() == D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE ||
               node.GetNodeType() == D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE;

  for (uint32_t i = 0, j = 0; i < node.GetChildCount(); ++i, ++j)
    i += RecurseNameGeneration(Refl, NodeId + 1 + i, j, self, isDotChild);

  if (isVar) {

    uint32_t typeId = node.GetLocalId();
    const ReflectionVariableType &type = Refl.Types[typeId];

    if (type.GetClass() == D3D_SVC_STRUCT)
      for (uint32_t i = 0; i < type.GetMemberCount(); ++i) {

        uint32_t memberId = i + type.GetMemberStart();
        std::string memberName =
            self + "." + Refl.Strings[Refl.MemberNameIds[memberId]];

        Refl.FullyResolvedToMemberId[memberName] = memberId;

        RecurseNameGenerationType(Refl, Refl.MemberTypeIds[memberId], i,
                                  memberName);
      }
  }

  return node.GetChildCount();
}

bool ReflectionData::GenerateNameLookupTable() {

  if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) || Nodes.empty())
    return false;

  NodeIdToFullyResolved.resize(Nodes.size());
  RecurseNameGeneration(*this, 0, 0, "", false);
  return true;
}

void ReflectionData::Dump(std::vector<std::byte> &Bytes) const {

  uint64_t toReserve = sizeof(HLSLReflectionDataHeader);

  Advance(toReserve, Strings, StringsNonDebug, Sources, Nodes, NodeSymbols,
          Registers, Functions, Enums, EnumValues, Annotations, ArraySizes,
          Arrays, MemberTypeIds, TypeList, MemberNameIds, Types, TypeSymbols,
          Buffers, Parameters, Statements, IfSwitchStatements,
          BranchStatements);

  Bytes.resize(toReserve);

  toReserve = 0;

  UnsafeCast<HLSLReflectionDataHeader>(Bytes, toReserve) = {
      ReflectionDataMagic,
      ReflectionDataVersion,
      uint16_t(Sources.size()),
      Features,
      uint32_t(StringsNonDebug.size()),
      uint32_t(Strings.size()),
      uint32_t(Nodes.size()),
      uint32_t(Registers.size()),
      uint32_t(Functions.size()),
      uint32_t(Enums.size()),
      uint32_t(EnumValues.size()),
      uint32_t(Annotations.size()),
      uint32_t(Arrays.size()),
      uint32_t(ArraySizes.size()),
      uint32_t(MemberTypeIds.size()),
      uint32_t(Types.size()),
      uint32_t(Buffers.size()),
      uint32_t(TypeList.size()),
      uint32_t(Parameters.size()),
      uint32_t(Statements.size()),
      uint32_t(IfSwitchStatements.size()),
      uint32_t(BranchStatements.size())};

  toReserve += sizeof(HLSLReflectionDataHeader);

  Append(Bytes, toReserve, Strings, StringsNonDebug, Sources, Nodes,
         NodeSymbols, Registers, Functions, Enums, EnumValues, Annotations,
         ArraySizes, Arrays, MemberTypeIds, TypeList, MemberNameIds, Types,
         TypeSymbols, Buffers, Parameters, Statements, IfSwitchStatements,
         BranchStatements);
}

D3D_CBUFFER_TYPE ReflectionData::GetBufferType(uint8_t Type) {

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

[[nodiscard]] ReflectionError
ReflectionData::Deserialize(const std::vector<std::byte> &Bytes,
                            bool MakeNameLookupTable) {

  *this = {};

  uint64_t off = 0;
  HLSLReflectionDataHeader header;
  if (ReflectionError err =
          Consume<HLSLReflectionDataHeader>(Bytes, off, header))
    return err;

  if (header.MagicNumber != ReflectionDataMagic)
    return HLSL_REFL_ERR("Invalid magic number");

  if (header.Version != ReflectionDataVersion)
    return HLSL_REFL_ERR("Unrecognized version number");

  Features = header.Features;

  bool hasSymbolInfo = Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!hasSymbolInfo && (header.Sources || header.Strings))
    return HLSL_REFL_ERR("Sources are invalid without symbols");

  uint32_t nodeSymbolCount = hasSymbolInfo ? header.Nodes : 0;
  uint32_t memberSymbolCount = hasSymbolInfo ? header.Members : 0;
  uint32_t typeSymbolCount = hasSymbolInfo ? header.Types : 0;

  if (ReflectionError err = Consume(
          Bytes, off, Strings, header.Strings, StringsNonDebug,
          header.StringsNonDebug, Sources, header.Sources, Nodes, header.Nodes,
          NodeSymbols, nodeSymbolCount, Registers, header.Registers, Functions,
          header.Functions, Enums, header.Enums, EnumValues, header.EnumValues,
          Annotations, header.Annotations, ArraySizes, header.ArraySizes,
          Arrays, header.Arrays, MemberTypeIds, header.Members, TypeList,
          header.TypeListCount, MemberNameIds, memberSymbolCount, Types,
          header.Types, TypeSymbols, typeSymbolCount, Buffers, header.Buffers,
          Parameters, header.Parameters, Statements, header.Statements,
          IfSwitchStatements, header.IfSwitchStatements, BranchStatements,
          header.BranchStatements))
    return err;

  // Validation errors to prevent accessing invalid data

  if (off != Bytes.size())
    return HLSL_REFL_ERR("Reflection info had unrecognized data on the back");

  for (uint32_t i = 0; i < header.Sources; ++i)
    if (Sources[i] >= header.Strings)
      return HLSL_REFL_ERR("Source path out of bounds", i);

  std::vector<uint32_t> validateChildren;

  for (uint32_t i = 0; i < header.Nodes; ++i) {

    const ReflectionNode &node = Nodes[i];

    if (hasSymbolInfo && (NodeSymbols[i].GetNameId() >= header.Strings ||
                          (NodeSymbols[i].GetFileSourceId() != uint16_t(-1) &&
                           NodeSymbols[i].GetFileSourceId() >= header.Sources)))
      return HLSL_REFL_ERR("Node points to invalid name or file name", i);

    if (node.GetAnnotationStart() + node.GetAnnotationCount() >
            header.Annotations ||
        node.GetNodeType() > D3D12_HLSL_NODE_TYPE_END ||
        (i && node.GetParentId() >= i) ||
        i + node.GetChildCount() > header.Nodes)
      return HLSL_REFL_ERR("Node is invalid", i);

    if (node.GetSemanticId() != uint32_t(-1) &&
        node.GetSemanticId() >= header.StringsNonDebug)
      return HLSL_REFL_ERR("Node points to invalid semantic id", i);

    if (node.GetInterpolationMode() < D3D_INTERPOLATION_UNDEFINED ||
        node.GetInterpolationMode() >
            D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE)
      return HLSL_REFL_ERR("Node has invalid interpolation mode", i);

    uint32_t maxValue = 1;
    bool allowFwdDeclare = false;

    switch (node.GetNodeType()) {
    case D3D12_HLSL_NODE_TYPE_REGISTER:
      maxValue = header.Registers;
      break;

    case D3D12_HLSL_NODE_TYPE_FUNCTION:
      maxValue = header.Functions;
      allowFwdDeclare = true;
      break;

    case D3D12_HLSL_NODE_TYPE_ENUM:
      maxValue = header.Enums;
      allowFwdDeclare = true;
      break;
    case D3D12_HLSL_NODE_TYPE_ENUM_VALUE:
      maxValue = header.EnumValues;
      break;

    case D3D12_HLSL_NODE_TYPE_PARAMETER:

      maxValue = header.Parameters;

      if (Nodes[node.GetParentId()].GetNodeType() !=
          D3D12_HLSL_NODE_TYPE_FUNCTION)
        return HLSL_REFL_ERR("Node is a parameter but parent isn't a function",
                             i);

      break;

    case D3D12_HLSL_NODE_TYPE_DEFAULT:
    case D3D12_HLSL_NODE_TYPE_CASE:

      maxValue = header.BranchStatements;

      if (Nodes[node.GetParentId()].GetNodeType() !=
          D3D12_HLSL_NODE_TYPE_SWITCH)
        return HLSL_REFL_ERR(
            "Node is a default/case but doesn't belong to a switch", i);
      break;

    case D3D12_HLSL_NODE_TYPE_IF_FIRST:
    case D3D12_HLSL_NODE_TYPE_ELSE_IF:
    case D3D12_HLSL_NODE_TYPE_ELSE:

      maxValue = header.BranchStatements;

      if (Nodes[node.GetParentId()].GetNodeType() !=
          D3D12_HLSL_NODE_TYPE_IF_ROOT)
        return HLSL_REFL_ERR(
            "Node is a if/else if/else but doesn't belong to an if root node",
            i);

      break;

    case D3D12_HLSL_NODE_TYPE_SCOPE:
    case D3D12_HLSL_NODE_TYPE_DO:
    case D3D12_HLSL_NODE_TYPE_IF_ROOT:
    case D3D12_HLSL_NODE_TYPE_FOR:
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_SWITCH:

      maxValue = node.GetNodeType() != D3D12_HLSL_NODE_TYPE_SCOPE &&
                         node.GetNodeType() != D3D12_HLSL_NODE_TYPE_DO
                     ? header.Statements
                     : 1;

      if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_SWITCH ||
          node.GetNodeType() == D3D12_HLSL_NODE_TYPE_IF_ROOT)
        maxValue = header.IfSwitchStatements;

      switch (Nodes[node.GetParentId()].GetNodeType()) {
      case D3D12_HLSL_NODE_TYPE_FUNCTION:
      case D3D12_HLSL_NODE_TYPE_IF_ROOT:
      case D3D12_HLSL_NODE_TYPE_SCOPE:
      case D3D12_HLSL_NODE_TYPE_DO:
      case D3D12_HLSL_NODE_TYPE_FOR:
      case D3D12_HLSL_NODE_TYPE_WHILE:
      case D3D12_HLSL_NODE_TYPE_SWITCH:
      case D3D12_HLSL_NODE_TYPE_CASE:
      case D3D12_HLSL_NODE_TYPE_DEFAULT:
      case D3D12_HLSL_NODE_TYPE_IF_FIRST:
      case D3D12_HLSL_NODE_TYPE_ELSE_IF:
      case D3D12_HLSL_NODE_TYPE_ELSE:
        break;

      default:
        return HLSL_REFL_ERR("Node is an stmt but "
                             "parent isn't of a similar "
                             "type or function",
                             i);
      }

      break;

    case D3D12_HLSL_NODE_TYPE_USING:
    case D3D12_HLSL_NODE_TYPE_TYPEDEF:
    case D3D12_HLSL_NODE_TYPE_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE:
      maxValue = header.Types;
      break;

    case D3D12_HLSL_NODE_TYPE_STRUCT:
    case D3D12_HLSL_NODE_TYPE_UNION:
    case D3D12_HLSL_NODE_TYPE_INTERFACE:
      allowFwdDeclare = true;
      maxValue = node.IsFwdDeclare() ? 1 : header.Types;
      break;
    }

    switch (node.GetNodeType()) {

    case D3D12_HLSL_NODE_TYPE_USING:
    case D3D12_HLSL_NODE_TYPE_TYPEDEF:
    case D3D12_HLSL_NODE_TYPE_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_PARAMETER:
      if (node.GetChildCount())
        return HLSL_REFL_ERR("Node is a parameter, typedef, variable or "
                             "static variable but also has children",
                             i);
      break;

    case D3D12_HLSL_NODE_TYPE_IF_ROOT:
    case D3D12_HLSL_NODE_TYPE_SCOPE:
    case D3D12_HLSL_NODE_TYPE_DO:
    case D3D12_HLSL_NODE_TYPE_FOR:
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_DEFAULT:
    case D3D12_HLSL_NODE_TYPE_CASE:
    case D3D12_HLSL_NODE_TYPE_IF_FIRST:
    case D3D12_HLSL_NODE_TYPE_ELSE_IF:
    case D3D12_HLSL_NODE_TYPE_ELSE:
      if (node.GetChildCount())
        validateChildren.push_back(i);
    }

    if ((node.GetNodeType() == D3D12_HLSL_NODE_TYPE_REGISTER ||
         node.GetNodeType() == D3D12_HLSL_NODE_TYPE_VARIABLE) &&
        Nodes[node.GetParentId()].GetNodeType() ==
            D3D12_HLSL_NODE_TYPE_INTERFACE)
      return HLSL_REFL_ERR("Node is interface but has registers or variables",
                           i);

    if (node.IsFwdDeclare() && !allowFwdDeclare)
      return HLSL_REFL_ERR("Node is fwd declare but that's not permitted", i);

    if (node.GetLocalId() >= maxValue)
      return HLSL_REFL_ERR("Node has invalid localId", i);
  }

  for (uint32_t i = 0; i < header.Registers; ++i) {

    const ReflectionShaderResource &reg = Registers[i];

    if (reg.GetNodeId() >= header.Nodes ||
        Nodes[reg.GetNodeId()].GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER ||
        Nodes[reg.GetNodeId()].GetLocalId() != i)
      return HLSL_REFL_ERR("Register points to an invalid nodeId", i);

    if (reg.GetType() > D3D_SIT_UAV_FEEDBACKTEXTURE ||
        reg.GetReturnType() > D3D_RETURN_TYPE_CONTINUED ||
        reg.GetDimension() > D3D_SRV_DIMENSION_BUFFEREX ||
        !reg.GetBindCount() ||
        (reg.GetArrayId() != uint32_t(-1) &&
         reg.GetArrayId() >= header.Arrays) ||
        (reg.GetArrayId() != uint32_t(-1) && reg.GetBindCount() <= 1))
      return HLSL_REFL_ERR(
          "Register invalid type, returnType, bindCount, array or dimension",
          i);

    D3D_CBUFFER_TYPE bufferType = ReflectionData::GetBufferType(reg.GetType());

    if (bufferType != D3D_CT_INTERFACE_POINTERS) {

      if (reg.GetBufferId() >= header.Buffers ||
          Buffers[reg.GetBufferId()].NodeId != reg.GetNodeId() ||
          Buffers[reg.GetBufferId()].Type != bufferType)
        return HLSL_REFL_ERR("Register invalid buffer referenced by register",
                             i);
    }
  }

  for (uint32_t i = 0; i < header.Functions; ++i) {

    const ReflectionFunction &func = Functions[i];

    if (func.GetNodeId() >= header.Nodes ||
        Nodes[func.GetNodeId()].GetNodeType() !=
            D3D12_HLSL_NODE_TYPE_FUNCTION ||
        Nodes[func.GetNodeId()].GetLocalId() != i)
      return HLSL_REFL_ERR("Function points to an invalid nodeId", i);

    uint32_t paramCount = func.GetNumParameters() + func.HasReturn();

    if (Nodes[func.GetNodeId()].GetChildCount() < paramCount)
      return HLSL_REFL_ERR("Function is missing parameters and/or return", i);

    for (uint32_t j = 0; j < paramCount; ++j)
      if (Nodes[func.GetNodeId() + 1 + j].GetParentId() != func.GetNodeId() ||
          Nodes[func.GetNodeId() + 1 + j].GetNodeType() !=
              D3D12_HLSL_NODE_TYPE_PARAMETER)
        return HLSL_REFL_ERR(
            "Function is missing valid parameters and/or return", i);
  }

  for (uint32_t i = 0; i < header.Enums; ++i) {

    const ReflectionEnumeration &enm = Enums[i];

    if (enm.NodeId >= header.Nodes ||
        Nodes[enm.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM ||
        Nodes[enm.NodeId].GetLocalId() != i)
      return HLSL_REFL_ERR("Function points to an invalid nodeId", i);

    if (enm.Type < D3D12_HLSL_ENUM_TYPE_START ||
        enm.Type > D3D12_HLSL_ENUM_TYPE_END)
      return HLSL_REFL_ERR("Enum has an invalid type", i);

    const ReflectionNode &node = Nodes[enm.NodeId];

    if (!node.IsFwdDeclare() && !node.GetChildCount())
      return HLSL_REFL_ERR("Enum has no values!", i);

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const ReflectionNode &child = Nodes[enm.NodeId + 1 + j];

      if (child.GetChildCount() != 0 ||
          child.GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM_VALUE)
        return HLSL_REFL_ERR("Enum has an invalid enum value", i);
    }
  }

  for (uint32_t i = 0; i < header.EnumValues; ++i) {

    const ReflectionEnumValue &enumVal = EnumValues[i];

    if (enumVal.NodeId >= header.Nodes ||
        Nodes[enumVal.NodeId].GetNodeType() !=
            D3D12_HLSL_NODE_TYPE_ENUM_VALUE ||
        Nodes[enumVal.NodeId].GetLocalId() != i ||
        Nodes[Nodes[enumVal.NodeId].GetParentId()].GetNodeType() !=
            D3D12_HLSL_NODE_TYPE_ENUM)
      return HLSL_REFL_ERR("Enum value points to an invalid nodeId", i);

    uint64_t maxVal = uint64_t(-1);
    ReflectionNode &parent = Nodes[Nodes[enumVal.NodeId].GetParentId()];

    switch (Enums[parent.GetLocalId()].Type) {
    case D3D12_HLSL_ENUM_TYPE_UINT:
    case D3D12_HLSL_ENUM_TYPE_INT:
      maxVal = uint32_t(-1);
      break;

    case D3D12_HLSL_ENUM_TYPE_UINT16_T:
    case D3D12_HLSL_ENUM_TYPE_INT16_T:
      maxVal = uint16_t(-1);
      break;
    }

    if (uint64_t(enumVal.Value) > maxVal)
      return HLSL_REFL_ERR("Enum value is invalid", i);
  }

  for (uint32_t i = 0; i < header.Arrays; ++i) {

    const ReflectionArray &arr = Arrays[i];

    if (arr.ArrayElem() <= 1 || arr.ArrayElem() > 32 ||
        arr.ArrayStart() + arr.ArrayElem() > header.ArraySizes)
      return HLSL_REFL_ERR("Array points to an invalid array element", i);
  }

  for (uint32_t i = 0; i < header.Annotations; ++i)
    if (Annotations[i].GetStringNonDebug() >= header.StringsNonDebug)
      return HLSL_REFL_ERR("Annotation points to an invalid string", i);

  for (uint32_t i = 0; i < header.Buffers; ++i) {

    const ReflectionShaderBuffer &buf = Buffers[i];

    if (buf.NodeId >= header.Nodes ||
        Nodes[buf.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER ||
        Nodes[buf.NodeId].GetLocalId() >= header.Registers ||
        Registers[Nodes[buf.NodeId].GetLocalId()].GetBufferId() != i)
      return HLSL_REFL_ERR("Buffer points to an invalid nodeId", i);

    const ReflectionNode &node = Nodes[buf.NodeId];

    if (!node.GetChildCount())
      return HLSL_REFL_ERR("Buffer requires at least one Variable child", i);

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const ReflectionNode &child = Nodes[buf.NodeId + 1 + j];

      if (child.GetChildCount() != 0 ||
          child.GetNodeType() != D3D12_HLSL_NODE_TYPE_VARIABLE)
        return HLSL_REFL_ERR("Buffer has to have only Variable child nodes", i);
    }
  }

  for (uint32_t i = 0; i < header.Members; ++i) {

    if (MemberTypeIds[i] >= header.Types)
      return HLSL_REFL_ERR("Member points to an invalid type", i);

    if (hasSymbolInfo && MemberNameIds[i] >= header.Strings)
      return HLSL_REFL_ERR("Member points to an invalid string", i);
  }

  for (uint32_t i = 0; i < header.TypeListCount; ++i)
    if (TypeList[i] >= header.Types)
      return HLSL_REFL_ERR("Type list index points to an invalid type", i);

  for (uint32_t i = 0; i < header.Parameters; ++i) {

    const ReflectionFunctionParameter &param = Parameters[i];

    if (param.NodeId >= header.Nodes ||
        Nodes[param.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_PARAMETER ||
        Nodes[param.NodeId].GetLocalId() != i || param.TypeId >= header.Types)
      return HLSL_REFL_ERR("Parameter points to an invalid nodeId", i);

    if (param.Flags > 3)
      return HLSL_REFL_ERR("Parameter has invalid data", i);
  }

  for (uint32_t nodeId : validateChildren) {

    const ReflectionNode &node = Nodes[nodeId];

    // If/Then/Scope children could only be
    // struct/union/interface/if/variable/typedef/enum

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const ReflectionNode &childNode = Nodes[nodeId + 1 + j];

      switch (childNode.GetNodeType()) {
      case D3D12_HLSL_NODE_TYPE_VARIABLE:
      case D3D12_HLSL_NODE_TYPE_IF_ROOT:
      case D3D12_HLSL_NODE_TYPE_STRUCT:
      case D3D12_HLSL_NODE_TYPE_UNION:
      case D3D12_HLSL_NODE_TYPE_INTERFACE:
      case D3D12_HLSL_NODE_TYPE_TYPEDEF:
      case D3D12_HLSL_NODE_TYPE_USING:
      case D3D12_HLSL_NODE_TYPE_ENUM:
      case D3D12_HLSL_NODE_TYPE_SCOPE:
      case D3D12_HLSL_NODE_TYPE_DO:
      case D3D12_HLSL_NODE_TYPE_FOR:
      case D3D12_HLSL_NODE_TYPE_WHILE:
      case D3D12_HLSL_NODE_TYPE_SWITCH:
      case D3D12_HLSL_NODE_TYPE_DEFAULT:
      case D3D12_HLSL_NODE_TYPE_CASE:
      case D3D12_HLSL_NODE_TYPE_IF_FIRST:
      case D3D12_HLSL_NODE_TYPE_ELSE_IF:
      case D3D12_HLSL_NODE_TYPE_ELSE:
        break;
      default:
        return HLSL_REFL_ERR(
            "Node has if/then/scope with children of invalid type", nodeId);
      }

      j += childNode.GetChildCount();
    }
  }

  for (uint32_t i = 0; i < header.Statements; ++i) {

    const ReflectionScopeStmt &Stmt = Statements[i];

    if (Stmt.GetNodeId() >= header.Nodes ||
        Nodes[Stmt.GetNodeId()].GetLocalId() != i)
      return HLSL_REFL_ERR("Statement points to an invalid nodeId", i);

    bool condVar = Stmt.HasConditionVar();
    uint32_t minParamCount = Stmt.GetNodeCount() + condVar;
    const ReflectionNode &node = Nodes[Stmt.GetNodeId()];

    if (node.GetChildCount() < minParamCount)
      return HLSL_REFL_ERR("Statement didn't have required child nodes", i);

    if (condVar && Nodes[Stmt.GetNodeId() + 1].GetNodeType() !=
                       D3D12_HLSL_NODE_TYPE_VARIABLE)
      return HLSL_REFL_ERR(
          "Statement has condition variable but first child is not a variable",
          i);

    switch (node.GetNodeType()) {
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_FOR:
      break;
    default:
      return HLSL_REFL_ERR("Statement has invalid node type", i);
    }
  }

  for (uint32_t i = 0; i < header.IfSwitchStatements; ++i) {

    const ReflectionIfSwitchStmt &Stmt = IfSwitchStatements[i];

    if (Stmt.GetNodeId() >= header.Nodes ||
        Nodes[Stmt.GetNodeId()].GetLocalId() != i)
      return HLSL_REFL_ERR("IfSwitchStmt points to an invalid nodeId", i);

    bool condVar = Stmt.HasConditionVar();
    uint32_t minParamCount = condVar + Stmt.HasElseOrDefault();
    const ReflectionNode &node = Nodes[Stmt.GetNodeId()];

    if (node.GetChildCount() < minParamCount)
      return HLSL_REFL_ERR("IfSwitchStmt didn't have required child nodes", i);

    if (condVar && node.GetNodeType() == D3D12_HLSL_NODE_TYPE_IF_ROOT)
      return HLSL_REFL_ERR("If statement can't have a conditional node in root",
                           i);

    if (condVar && Nodes[Stmt.GetNodeId() + 1].GetNodeType() !=
                       D3D12_HLSL_NODE_TYPE_VARIABLE)
      return HLSL_REFL_ERR(
          "Statement has condition variable but first child is not a variable",
          i);

    switch (node.GetNodeType()) {
    case D3D12_HLSL_NODE_TYPE_IF_ROOT:
    case D3D12_HLSL_NODE_TYPE_SWITCH:
      break;
    default:
      return HLSL_REFL_ERR("IfSwitchStmt has invalid node type", i);
    }

    bool isIf = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_IF_ROOT;

    // Ensure there's only one default/else and the first is the IF_FIRST node.

    uint32_t nodeStart = Stmt.GetNodeId() + 1 + condVar;
    uint32_t nodeEnd = Stmt.GetNodeId() + node.GetChildCount();
    bool hasSingleNode =
        false; // Else or default where you're only allowed to have one

    for (uint32_t j = nodeStart, k = 0; j < nodeEnd; ++j, ++k) {

      const ReflectionNode &child = Nodes[j];

      bool isSingleNode =
          child.GetNodeType() ==
          (isIf ? D3D12_HLSL_NODE_TYPE_ELSE : D3D12_HLSL_NODE_TYPE_DEFAULT);

      if (isSingleNode) {

        if (hasSingleNode)
          return HLSL_REFL_ERR("IfSwitchStmt already has default/else", i);

        if (isIf && !k)
          return HLSL_REFL_ERR("IfSwitchStmt started with else", i);

        hasSingleNode = true;
      }

      else {

        D3D12_HLSL_NODE_TYPE expected =
            !isIf ? D3D12_HLSL_NODE_TYPE_CASE
                  : (!k ? D3D12_HLSL_NODE_TYPE_IF_FIRST
                        : D3D12_HLSL_NODE_TYPE_ELSE_IF);

        if (child.GetNodeType() != expected)
          return HLSL_REFL_ERR("IfSwitchStmt has an invalid member", i);
      }

      j += child.GetChildCount();
    }
  }

  for (uint32_t i = 0; i < header.BranchStatements; ++i) {

    const ReflectionBranchStmt &Stmt = BranchStatements[i];

    if (Stmt.GetNodeId() >= header.Nodes ||
        Nodes[Stmt.GetNodeId()].GetLocalId() != i)
      return HLSL_REFL_ERR("BranchStatements points to an invalid nodeId", i);

    if (Stmt.GetValueType() < D3D12_HLSL_ENUM_TYPE_UINT ||
        Stmt.GetValueType() > D3D12_HLSL_ENUM_TYPE_INT16_T)
      return HLSL_REFL_ERR("BranchStatement has an invalid value type", i);

    bool condVar = Stmt.HasConditionVar();
    uint32_t minParamCount = condVar;
    const ReflectionNode &node = Nodes[Stmt.GetNodeId()];

    if (node.GetChildCount() < minParamCount)
      return HLSL_REFL_ERR("IfSwitchStmt didn't have required child nodes", i);

    if (condVar && (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_DEFAULT ||
                    node.GetNodeType() == D3D12_HLSL_NODE_TYPE_CASE ||
                    node.GetNodeType() == D3D12_HLSL_NODE_TYPE_ELSE))
      return HLSL_REFL_ERR(
          "Default, case or else can't have a conditional node in root", i);

    if (condVar && Nodes[Stmt.GetNodeId() + 1].GetNodeType() !=
                       D3D12_HLSL_NODE_TYPE_VARIABLE)
      return HLSL_REFL_ERR(
          "Statement has condition variable but first child is not a variable",
          i);

    switch (node.GetNodeType()) {
    case D3D12_HLSL_NODE_TYPE_CASE:
    case D3D12_HLSL_NODE_TYPE_DEFAULT:
    case D3D12_HLSL_NODE_TYPE_IF_FIRST:
    case D3D12_HLSL_NODE_TYPE_ELSE_IF:
    case D3D12_HLSL_NODE_TYPE_ELSE:
      break;
    default:
      return HLSL_REFL_ERR("IfSwitchStmt has invalid node type", i);
    }

    if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_CASE &&
        !Stmt.IsComplexCase()) {

      uint64_t maxVal = uint64_t(-1);

      switch (Stmt.GetValueType()) {

      case D3D12_HLSL_ENUM_TYPE_UINT:
      case D3D12_HLSL_ENUM_TYPE_INT:
        maxVal = uint32_t(-1);
        break;

      case D3D12_HLSL_ENUM_TYPE_UINT16_T:
      case D3D12_HLSL_ENUM_TYPE_INT16_T:
        maxVal = uint16_t(-1);
        break;
      }

      if (Stmt.GetValue() > maxVal)
        return HLSL_REFL_ERR("IfSwitchStmt is out of bounds for the value type",
                             i);
    }

    else if (Stmt.GetValue() != uint64_t(-1))
      return HLSL_REFL_ERR("IfSwitchStmt should have -1 as value", i);
  }

  for (uint32_t i = 0; i < header.Types; ++i) {

    const ReflectionVariableType &type = Types[i];

    if (hasSymbolInfo && (TypeSymbols[i].DisplayNameId >= header.Strings ||
                          TypeSymbols[i].UnderlyingNameId >= header.Strings))
      return HLSL_REFL_ERR("Type points to an invalid string", i);

    if (hasSymbolInfo && (TypeSymbols[i].DisplayArray.ElementsOrArrayId >> 31 &&
                          (TypeSymbols[i].DisplayArray.ElementsOrArrayId << 1 >>
                           1) >= header.Arrays))
      return HLSL_REFL_ERR("Type points to an invalid string", i);

    if ((type.GetBaseClass() != uint32_t(-1) &&
         type.GetBaseClass() >= header.Types) ||
        type.GetMemberStart() + type.GetMemberCount() > header.Members ||
        type.GetInterfaceStart() + type.GetInterfaceCount() >
            header.TypeListCount ||
        (type.GetUnderlyingArray().ElementsOrArrayId >> 31 &&
         (type.GetUnderlyingArray().ElementsOrArrayId << 1 >> 1) >=
             header.Arrays))
      return HLSL_REFL_ERR(
          "Type points to an invalid string, array, base class or member", i);

    switch (type.GetClass()) {

    case D3D_SVC_SCALAR:

      if (type.GetColumns() != 1)
        return HLSL_REFL_ERR("Type (scalar) should have columns == 1", i);

      [[fallthrough]];

    case D3D_SVC_VECTOR:

      if (type.GetRows() != 1)
        return HLSL_REFL_ERR("Type (scalar/vector) should have rows == 1", i);

      [[fallthrough]];

    case D3D_SVC_MATRIX_ROWS:
    case D3D_SVC_MATRIX_COLUMNS:

      if (!type.GetRows() || !type.GetColumns() || type.GetRows() > 128 ||
          type.GetColumns() > 128)
        return HLSL_REFL_ERR(
            "Type (scalar/vector/matrix) has invalid rows or columns", i);

      switch (type.GetType()) {
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
        return HLSL_REFL_ERR("Type (scalar/matrix/vector) is of invalid type",
                             i);
      }

      break;

    case D3D_SVC_STRUCT:
      [[fallthrough]];

    case D3D_SVC_INTERFACE_CLASS:

      if (type.GetType())
        return HLSL_REFL_ERR("Type (struct) shouldn't have rows or columns", i);

      if (type.GetRows() || type.GetColumns())
        return HLSL_REFL_ERR("Type (struct) shouldn't have rows or columns", i);

      break;

    case D3D_SVC_OBJECT:

      switch (type.GetType()) {

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
        return HLSL_REFL_ERR("Type (object) is of invalid type", i);
      }

      if (type.GetRows() || type.GetColumns())
        return HLSL_REFL_ERR("Type (object) shouldn't have rows or columns", i);

      break;

    default:
      return HLSL_REFL_ERR("Type has an invalid class", i);
    }
  }

  // Validate fwd & backwards declares

  for (uint32_t i = 0; i < header.Nodes; ++i) {

    const ReflectionNode &node = Nodes[i];

    if (node.IsFwdBckDefined()) {

      uint32_t fwdBack = node.GetFwdBck();

      if (Nodes[fwdBack].GetNodeType() != node.GetNodeType())
        return HLSL_REFL_ERR("Node (fwd/bck declare) points to element that of "
                             "incompatible type",
                             i);

      if (hasSymbolInfo &&
          NodeSymbols[fwdBack].GetNameId() != NodeSymbols[i].GetNameId())
        return HLSL_REFL_ERR("Node (fwd/bck declare) have mismatching name", i);

      if (node.IsFwdDeclare()) {

        if (fwdBack <= i || fwdBack >= header.Nodes)
          return HLSL_REFL_ERR("Node (fwd declare) points to invalid element",
                               i);

        if (Nodes[fwdBack].IsFwdDeclare())
          return HLSL_REFL_ERR(
              "Node (fwd declare) points to element that is also a fwd declare",
              i);

        uint32_t paramCount = 0;

        if (node.GetNodeType() == D3D12_HLSL_NODE_TYPE_FUNCTION) {
          const ReflectionFunction &func = Functions[node.GetLocalId()];
          paramCount = func.GetNumParameters() + func.HasReturn();
        }

        if ((node.GetChildCount() != paramCount) || node.GetAnnotationCount())
          return HLSL_REFL_ERR(
              "Node (fwd declare) points to element with invalid child count, "
              "or annotationCount",
              i);
      }

      else {

        if (fwdBack >= i)
          return HLSL_REFL_ERR("Node (bck declare) points to invalid element",
                               i);

        if (!Nodes[fwdBack].IsFwdDeclare())
          return HLSL_REFL_ERR(
              "Node (bck declare) points to element that is not a fwd declare",
              i);
      }
    }
  }

  // Finalize

  if (MakeNameLookupTable)
    GenerateNameLookupTable();

  return ReflectionErrorSuccess;
}

} // namespace hlsl
