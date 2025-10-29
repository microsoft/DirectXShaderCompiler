///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflection.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxcReflection/DxcReflection.h"
#include <inttypes.h>
#include <stdexcept>

#undef min
#undef max

namespace hlsl {
  
[[nodiscard]] DxcReflectionError DxcHLSLReflectionData::RegisterString(
    uint32_t &StringId,
    const std::string &Name,
                                               bool IsNonDebug) {

  if (Name.size() >= 32768)
    return DXC_REFLECT_ERR("Strings are limited to 32767");

  if (IsNonDebug) {

    if (StringsNonDebug.size() >= uint32_t(-1))
      return DXC_REFLECT_ERR("Strings overflow");

    auto it = StringsToIdNonDebug.find(Name);

    if (it != StringsToIdNonDebug.end()) {
      StringId = it->second;
      return DxcReflectionSuccess;
    }

    uint32_t stringId = uint32_t(StringsNonDebug.size());

    StringsNonDebug.push_back(Name);
    StringsToIdNonDebug[Name] = stringId;
    StringId = stringId;
    return DxcReflectionSuccess;
  }

  if (Strings.size() >= uint32_t(-1))
    return DXC_REFLECT_ERR("Strings overflow");

  auto it = StringsToId.find(Name);

  if (it != StringsToId.end()) {
    StringId = it->second;
    return DxcReflectionSuccess;
  }

  uint32_t stringId = uint32_t(Strings.size());

  Strings.push_back(Name);
  StringsToId[Name] = stringId;
  StringId = stringId;
  return DxcReflectionSuccess;
}

[[nodiscard]] DxcReflectionError
DxcHLSLReflectionData::PushArray(uint32_t &ArrayId, uint32_t ArraySizeFlat,
                                 const std::vector<uint32_t> &ArraySize) {

  if (ArraySizeFlat <= 1 || ArraySize.size() <= 1) {
    ArrayId = uint32_t(-1);
    return DxcReflectionSuccess;
  }

  if (Arrays.size() >= uint32_t((1u << 31) - 1))
    return DXC_REFLECT_ERR("Arrays would overflow");

  uint32_t arrayId = uint32_t(Arrays.size());

  uint32_t arrayCountStart = uint32_t(ArraySizes.size());
  uint32_t numArrayElements = std::min(size_t(32), ArraySize.size());

  if (ArraySizes.size() + numArrayElements >= ((1u << 26) - 1))
    return DXC_REFLECT_ERR("Array elements would overflow");

  for (uint32_t i = 0; i < ArraySize.size() && i < 8; ++i) {

    uint32_t arraySize = ArraySize[i];

    // Flatten rest of array to at least keep consistent array elements
    if (i == 31)
      for (uint32_t j = i + 1; j < ArraySize.size(); ++j)
        arraySize *= ArraySize[j];

    ArraySizes.push_back(arraySize);
  }

  DxcHLSLArray arr;

  if (DxcReflectionError err =
          DxcHLSLArray::Initialize(arr, numArrayElements, arrayCountStart))
    return err;

  for (uint32_t i = 0; i < Arrays.size(); ++i)
    if (Arrays[i] == arr) {
      ArrayId = i;
      return DxcReflectionSuccess;
    }

  Arrays.push_back(arr);
  ArrayId = arrayId;
  return DxcReflectionSuccess;
}

[[nodiscard]] DxcReflectionError
DxcHLSLReflectionData::RegisterTypeList(
    const std::vector<uint32_t> &TypeIds, uint32_t &Offset, uint8_t &Len) {

  if (TypeIds.empty())
    return DxcReflectionSuccess;

  if (TypeIds.size() >= uint8_t(-1))
    return DXC_REFLECT_ERR("Only allowing 256 types in a type list");

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
      return DXC_REFLECT_ERR("Only allowing 16Mi total interfaces");

    TypeList.resize(oldSiz + TypeIds.size());

    std::memcpy(TypeList.data() + oldSiz, TypeIds.data(),
                TypeIds.size() * sizeof(uint32_t));

    Offset = oldSiz;
  }

  Len = uint8_t(TypeIds.size());
  return DxcReflectionSuccess;
}

//TODO: Debug print code

static std::string RegisterGetArraySize(const DxcHLSLReflectionData &Refl, const DxcHLSLRegister &reg) {

  if (reg.GetArrayId() != (uint32_t)-1) {

    DxcHLSLArray arr = Refl.Arrays[reg.GetArrayId()];
    std::string str;

    for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
      str += "[" + std::to_string(Refl.ArraySizes[arr.ArrayStart() + i]) + "]";

    return str;
  }

  return reg.GetBindCount() > 1 ? "[" + std::to_string(reg.GetBindCount()) + "]"
                                : "";
}

static std::string EnumTypeToString(D3D12_HLSL_ENUM_TYPE type) {

  static const char *arr[] = {
      "uint", "int", "uint64_t", "int64_t", "uint16_t", "int16_t",
  };

  return arr[type];
}

static std::string NodeTypeToString(D3D12_HLSL_NODE_TYPE type) {

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

  return arr[uint32_t(type)];
}

static std::string
PrintArray(const DxcHLSLReflectionData &Refl, const DxcHLSLArrayOrElements &Arr) {
    
  std::string result;

  if (Arr.IsMultiDimensionalArray()) {

    const DxcHLSLArray &arr = Refl.Arrays[Arr.GetMultiDimensionalArrayId()];

    for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
      result +=
          "[" + std::to_string(Refl.ArraySizes[arr.ArrayStart() + i]) + "]";

  }

  else if (Arr.IsArray())
    result += "[" + std::to_string(Arr.Get1DElements()) + "]";

  return result;
}

static std::string GetBuiltinTypeName(const DxcHLSLReflectionData &Refl,
                               const DxcHLSLType &Type) {

  std::string type;

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

static std::string PrintTypeInfo(const DxcHLSLReflectionData &Refl,
                                 const DxcHLSLType &Type,
                                 const DxcHLSLTypeSymbol &Symbol,
                                 const std::string &PreviousTypeName) {

  std::string result = PrintArray(Refl, Type.GetUnderlyingArray());

  // Obtain type name (returns empty if it's not a builtin type)

  if (Symbol.DisplayNameId != Symbol.UnderlyingNameId &&
      Symbol.UnderlyingNameId)
    result += " (" + Refl.Strings[Symbol.DisplayNameId] +
              PrintArray(Refl, Symbol.DisplayArray) + ")";

  return result;
}

static void RecursePrintType(const DxcHLSLReflectionData &Refl, uint32_t TypeId,
                      uint32_t Depth, const char *Prefix) {

  const DxcHLSLType &type = Refl.Types[TypeId];

  bool hasSymbols = Refl.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  DxcHLSLTypeSymbol symbol =
      hasSymbols ? Refl.TypeSymbols[TypeId] : DxcHLSLTypeSymbol();

  std::string name = hasSymbols ? Refl.Strings[symbol.UnderlyingNameId]
                                : GetBuiltinTypeName(Refl, type);

  if (name.empty() && !hasSymbols)
    name = "(unknown)";

  printf("%s%s%s%s\n", std::string(Depth, '\t').c_str(), Prefix, name.c_str(),
         PrintTypeInfo(Refl, type, symbol, name).c_str());

  if (type.GetBaseClass() != uint32_t(-1))
    RecursePrintType(Refl, type.GetBaseClass(), Depth + 1,
                     (std::string("BaseClass ") + Prefix).c_str());

  for (uint32_t i = 0; i < type.GetInterfaceCount(); ++i) {

    uint32_t interfaceId = type.GetInterfaceStart() + i;

    RecursePrintType(Refl, Refl.TypeList[interfaceId], Depth + 1, "Interface ");
  }

  for (uint32_t i = 0; i < type.GetMemberCount(); ++i) {

    uint32_t memberId = type.GetMemberStart() + i;
    std::string prefix =
        (hasSymbols ? Refl.Strings[Refl.MemberNameIds[memberId]] : "(unknown)") + ": ";

    RecursePrintType(Refl, Refl.MemberTypeIds[memberId], Depth + 1, prefix.c_str());
  }
}

uint32_t RecursePrint(const DxcHLSLReflectionData &Refl, uint32_t NodeId,
                      uint32_t Depth, uint32_t IndexInParent, bool isThroughFwdDecl = false) {

  const DxcHLSLNode &node = Refl.Nodes[NodeId];

  uint32_t typeToPrint = (uint32_t)-1;

  if (NodeId) {

    bool hasSymbols = Refl.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

    if (!isThroughFwdDecl)
      printf("%s%s %s%s\n", std::string(Depth - 1, '\t').c_str(),
             NodeTypeToString(node.GetNodeType()).c_str(),
             hasSymbols
                 ? Refl.Strings[Refl.NodeSymbols[NodeId].GetNameId()].c_str()
                        : "(unknown)",
             node.IsFwdDeclare() ? " (declaration)"
                                 : (!isThroughFwdDecl && node.IsFwdBckDefined()
                                        ? " (definition)"
                                        : ""));

    if (node.IsFwdBckDefined() && !node.IsFwdDeclare() && !isThroughFwdDecl)
      return node.GetChildCount();

    for (uint32_t i = 0; i < node.GetAnnotationCount(); ++i) {

      const DxcHLSLAnnotation &annotation =
          Refl.Annotations[node.GetAnnotationStart() + i];

      printf(annotation.GetIsBuiltin() ? "%s[%s]\n" : "%s[[%s]]\n",
             std::string(Depth, '\t').c_str(),
             Refl.StringsNonDebug[annotation.GetStringNonDebug()].c_str());
    }

    if (node.GetSemanticId() != uint32_t(-1))
      printf("%s: %s\n", std::string(Depth, '\t').c_str(),
             Refl.StringsNonDebug[node.GetSemanticId()].c_str());

    uint32_t localId = node.GetLocalId();

    if (!node.IsFwdDeclare())
      switch (node.GetNodeType()) {

      case D3D12_HLSL_NODE_TYPE_REGISTER: {

        const DxcHLSLRegister &reg = Refl.Registers[localId];

        if (reg.GetArrayId() == (uint32_t)-1 && reg.GetBindCount() == 1)
          break;

        printf("%s%s\n", std::string(Depth, '\t').c_str(),
               RegisterGetArraySize(Refl, reg).c_str());
        break;
      }

      case D3D12_HLSL_NODE_TYPE_UNION:
      case D3D12_HLSL_NODE_TYPE_STRUCT: { // Children are Variables

        const DxcHLSLType &type = Refl.Types[localId];

        if (type.GetBaseClass() != uint32_t(-1))
          RecursePrintType(Refl, type.GetBaseClass(), Depth, "BaseClass ");

        for (uint32_t i = 0; i < type.GetInterfaceCount(); ++i) {

          uint32_t interfaceId = type.GetInterfaceStart() + i;

          RecursePrintType(Refl, Refl.TypeList[interfaceId], Depth,
                           "Interface ");
        }

        break;
      }

      case D3D12_HLSL_NODE_TYPE_INTERFACE:
        break;

        // TODO: case D3D12_HLSL_NODE_TYPE_USING:

      case D3D12_HLSL_NODE_TYPE_PARAMETER: {

        const DxcHLSLParameter &param = Refl.Parameters[localId];

        if (param.InterpolationMode || param.Flags) {

          static const char *inouts[] = {NULL, "in", "out", "inout"};
          const char *inout = inouts[param.Flags];

          static const char *interpolationModes[] = {
              "",
              "Constant",
              "Linear",
              "LinearCentroid",
              "LinearNoPerspective",
              "LinearNoPerspectiveCentroid",
              "LinearSample",
              "LinearNoPerspectiveSample"};

          const char *interpolationMode =
              interpolationModes[param.InterpolationMode];

          printf("%s%s%s\n", std::string(Depth, '\t').c_str(), inout ? inout : "",
                 ((inout ? " " : "") + std::string(interpolationMode)).c_str());
        }

        typeToPrint = param.TypeId;
        break;
      }

      case D3D12_HLSL_NODE_TYPE_WHILE:
      case D3D12_HLSL_NODE_TYPE_FOR:
      case D3D12_HLSL_NODE_TYPE_SWITCH:
      case D3D12_HLSL_NODE_TYPE_IF: {

        const DxcHLSLStatement &Stmt = Refl.Statements[localId];
        const DxcHLSLNode &Node = Refl.Nodes[Stmt.GetNodeId()];

        uint32_t bodyNodes =
            Node.GetChildCount() - Stmt.GetNodeCount() - Stmt.HasConditionVar();

        bool isIf = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_IF;
        bool isFor = node.GetNodeType() == D3D12_HLSL_NODE_TYPE_FOR;

        if (bodyNodes || Stmt.GetNodeCount() || Stmt.HasConditionVar() ||
            Stmt.HasElse()) {

          if (isIf || isFor)
            printf("%s%snodes: %u, %snodes: %u%s%s\n",
                   std::string(Depth, '\t').c_str(), isIf ? "if " : "init ",
                   Stmt.GetNodeCount(), isIf ? "else " : "body ", bodyNodes,
                   Stmt.HasConditionVar() ? " (has cond var)" : "",
                   Stmt.HasElse() ? " (has else)" : "");

          else
            printf("%sbody nodes: %u%s\n", std::string(Depth, '\t').c_str(),
                   bodyNodes, Stmt.HasConditionVar() ? " (has cond var)" : "");
        }

        break;
      }

      case D3D12_HLSL_NODE_TYPE_TYPEDEF:
      case D3D12_HLSL_NODE_TYPE_VARIABLE:
      case D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE:
      case D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE:
        typeToPrint = localId;
        break;

      case D3D12_HLSL_NODE_TYPE_FUNCTION: {
        const DxcHLSLFunction &func = Refl.Functions[localId];
        printf("%sreturn: %s, hasDefinition: %s, numParams: %u\n",
               std::string(Depth, '\t').c_str(),
               func.HasReturn() ? "true" : "false",
               func.HasDefinition() ? "true" : "false",
               func.GetNumParameters());

        break;
      }

      case D3D12_HLSL_NODE_TYPE_ENUM:
        printf("%s: %s\n", std::string(Depth, '\t').c_str(),
               EnumTypeToString(Refl.Enums[localId].Type).c_str());
        break;

      case D3D12_HLSL_NODE_TYPE_ENUM_VALUE: {
        printf("%s#%u = %" PRIi64 "\n", std::string(Depth, '\t').c_str(),
               IndexInParent, Refl.EnumValues[localId].Value);
        break;
      }

      case D3D12_HLSL_NODE_TYPE_NAMESPACE:
      case D3D12_HLSL_NODE_TYPE_SCOPE:
      case D3D12_HLSL_NODE_TYPE_DO:
      default:
        break;
      }
  }

  if (typeToPrint != (uint32_t)-1)
    RecursePrintType(Refl, typeToPrint, Depth, "");

  for (uint32_t i = 0, j = 0; i < node.GetChildCount(); ++i, ++j)
    i += RecursePrint(Refl, NodeId + 1 + i, Depth + 1, j);

  if (node.IsFwdDeclare() && node.IsFwdBckDefined())
    RecursePrint(Refl, node.GetFwdBck(), Depth, IndexInParent, true);

  return node.GetChildCount();
}

struct DxcHLSLHeader {

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

template <typename T>
void SkipPadding(uint64_t &Offset) {
  Offset = (Offset + alignof(T) - 1) / alignof(T) * alignof(T);
}

template <typename T>
void Skip(uint64_t &Offset, const std::vector<T> &Vec) {
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

void DxcHLSLReflectionData::StripSymbols() {
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

void RecurseNameGenerationType(DxcHLSLReflectionData &Refl, uint32_t TypeId,
                               uint32_t LocalId, const std::string &Parent) {

  const DxcHLSLType &type = Refl.Types[TypeId];

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

uint32_t RecurseNameGeneration(DxcHLSLReflectionData &Refl, uint32_t NodeId,
                               uint32_t LocalId, const std::string &Parent,
                               bool IsDot) {

  DxcHLSLNode node = Refl.Nodes[NodeId];

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
      const DxcHLSLType &type = Refl.Types[typeId];

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

bool DxcHLSLReflectionData::GenerateNameLookupTable() {

  if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) || Nodes.empty())
      return false;

  NodeIdToFullyResolved.resize(Nodes.size());
  RecurseNameGeneration(*this, 0, 0, "", false);
  return true;
}

void DxcHLSLReflectionData::Dump(std::vector<std::byte> &Bytes) const {

  uint64_t toReserve = sizeof(DxcHLSLHeader);

  Advance(toReserve, Strings, StringsNonDebug, Sources, Nodes, NodeSymbols,
          Registers, Functions, Enums, EnumValues, Annotations, ArraySizes,
          Arrays, MemberTypeIds, TypeList, MemberNameIds, Types, TypeSymbols,
          Buffers, Parameters, Statements);

  Bytes.resize(toReserve);

  toReserve = 0;

  UnsafeCast<DxcHLSLHeader>(Bytes, toReserve) = {
      DxcReflectionDataMagic,           DxcReflectionDataVersion,
      uint16_t(Sources.size()),         Features,
      uint32_t(StringsNonDebug.size()), uint32_t(Strings.size()),
      uint32_t(Nodes.size()),           uint32_t(Registers.size()),
      uint32_t(Functions.size()),       uint32_t(Enums.size()),
      uint32_t(EnumValues.size()),      uint32_t(Annotations.size()),
      uint32_t(Arrays.size()),          uint32_t(ArraySizes.size()),
      uint32_t(MemberTypeIds.size()),   uint32_t(Types.size()),
      uint32_t(Buffers.size()),         uint32_t(TypeList.size()),
      uint32_t(Parameters.size()),      uint32_t(Statements.size())};

  toReserve += sizeof(DxcHLSLHeader);

  Append(Bytes, toReserve, Strings, StringsNonDebug, Sources, Nodes,
         NodeSymbols, Registers, Functions, Enums, EnumValues, Annotations,
         ArraySizes, Arrays, MemberTypeIds, TypeList, MemberNameIds, Types,
         TypeSymbols, Buffers, Parameters, Statements);
}

D3D_CBUFFER_TYPE DxcHLSLReflectionData::GetBufferType(uint8_t Type) {

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

DxcHLSLReflectionData::DxcHLSLReflectionData(const std::vector<std::byte> &Bytes,
                                     bool MakeNameLookupTable) {

  uint64_t off = 0;
  DxcHLSLHeader header;
  Consume<DxcHLSLHeader>(Bytes, off, header);

  if (header.MagicNumber != DxcReflectionDataMagic)
    throw std::invalid_argument("Invalid magic number");

  if (header.Version != DxcReflectionDataVersion)
    throw std::invalid_argument("Unrecognized version number");

  Features = header.Features;

  bool hasSymbolInfo = Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!hasSymbolInfo && (header.Sources || header.Strings))
    throw std::invalid_argument("Sources are invalid without symbols");

  uint32_t nodeSymbolCount = hasSymbolInfo ? header.Nodes : 0;
  uint32_t memberSymbolCount = hasSymbolInfo ? header.Members : 0;
  uint32_t typeSymbolCount = hasSymbolInfo ? header.Types : 0;

  Consume(Bytes, off, Strings, header.Strings, StringsNonDebug,
          header.StringsNonDebug, Sources, header.Sources, Nodes, header.Nodes,
          NodeSymbols, nodeSymbolCount, Registers, header.Registers, Functions,
          header.Functions, Enums, header.Enums, EnumValues, header.EnumValues,
          Annotations, header.Annotations, ArraySizes, header.ArraySizes,
          Arrays, header.Arrays, MemberTypeIds, header.Members, TypeList,
          header.TypeListCount, MemberNameIds, memberSymbolCount, Types,
          header.Types, TypeSymbols, typeSymbolCount, Buffers, header.Buffers,
          Parameters, header.Parameters, Statements, header.Statements);

  // Validation errors are throws to prevent accessing invalid data

  if (off != Bytes.size())
    throw std::invalid_argument("Reflection info had unrecognized data on the back");

  for(uint32_t i = 0; i < header.Sources; ++i)
    if(Sources[i] >= header.Strings)
      throw std::invalid_argument("Source path out of bounds");

  std::vector<uint32_t> validateChildren;

  for (uint32_t i = 0; i < header.Nodes; ++i) {

    const DxcHLSLNode &node = Nodes[i];

    if (hasSymbolInfo && (NodeSymbols[i].GetNameId() >= header.Strings ||
                          (NodeSymbols[i].GetFileSourceId() != uint16_t(-1) &&
                           NodeSymbols[i].GetFileSourceId() >= header.Sources)))
      throw std::invalid_argument("Node " + std::to_string(i) +
                                  " points to invalid name or file name");

    if (node.GetAnnotationStart() + node.GetAnnotationCount() >
            header.Annotations ||
        node.GetNodeType() > D3D12_HLSL_NODE_TYPE_END ||
        (i && node.GetParentId() >= i) ||
        i + node.GetChildCount() > header.Nodes)
      throw std::invalid_argument("Node " + std::to_string(i) + " is invalid");

    if (node.GetSemanticId() != uint32_t(-1) &&
        node.GetSemanticId() >= header.StringsNonDebug)
      throw std::invalid_argument("Node " + std::to_string(i) + " points to invalid semantic id");

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
        throw std::invalid_argument(
            "Node " + std::to_string(i) +
            " is a parameter but parent isn't a function");

      break;

    case D3D12_HLSL_NODE_TYPE_SCOPE:
    case D3D12_HLSL_NODE_TYPE_DO:
    case D3D12_HLSL_NODE_TYPE_IF:
    case D3D12_HLSL_NODE_TYPE_FOR:
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_SWITCH:

      maxValue = node.GetNodeType() != D3D12_HLSL_NODE_TYPE_SCOPE &&
                         node.GetNodeType() != D3D12_HLSL_NODE_TYPE_DO
                     ? header.Statements
                     : 1;

      switch (Nodes[node.GetParentId()].GetNodeType()) {
      case D3D12_HLSL_NODE_TYPE_FUNCTION:
      case D3D12_HLSL_NODE_TYPE_IF:
      case D3D12_HLSL_NODE_TYPE_SCOPE:
      case D3D12_HLSL_NODE_TYPE_DO:
      case D3D12_HLSL_NODE_TYPE_FOR:
      case D3D12_HLSL_NODE_TYPE_WHILE:
      case D3D12_HLSL_NODE_TYPE_SWITCH:
        break;

      default:
        throw std::invalid_argument(
            "Node " + std::to_string(i) +
            " is an if/scope/do/for/while/switch but parent isn't of a similar "
            "type or function");
      }

      break;

    // TODO: case D3D12_HLSL_NODE_TYPE_USING:
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

    case D3D12_HLSL_NODE_TYPE_TYPEDEF:
    case D3D12_HLSL_NODE_TYPE_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE:
    case D3D12_HLSL_NODE_TYPE_PARAMETER:
      if (node.GetChildCount())
        throw std::invalid_argument("Node " + std::to_string(i) +
                                    " is a parameter, typedef, variable or "
                                    "static variable but also has children");

    case D3D12_HLSL_NODE_TYPE_IF:
    case D3D12_HLSL_NODE_TYPE_SCOPE:
    case D3D12_HLSL_NODE_TYPE_DO:
    case D3D12_HLSL_NODE_TYPE_FOR:
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_SWITCH:
      if (node.GetChildCount())
        validateChildren.push_back(i);
    }

    if ((node.GetNodeType() == D3D12_HLSL_NODE_TYPE_REGISTER ||
         node.GetNodeType() == D3D12_HLSL_NODE_TYPE_VARIABLE) &&
        Nodes[node.GetParentId()].GetNodeType() ==
            D3D12_HLSL_NODE_TYPE_INTERFACE)
      throw std::invalid_argument(
          "Node " + std::to_string(i) +
          " is interface but has registers or variables");

    if (node.IsFwdDeclare() && !allowFwdDeclare)
      throw std::invalid_argument("Node " + std::to_string(i) +
                                  " is fwd declare but that's not permitted");

    if (node.GetLocalId() >= maxValue)
      throw std::invalid_argument("Node " + std::to_string(i) +
                                  " has invalid localId");
  }

  for(uint32_t i = 0; i < header.Registers; ++i) {

    const DxcHLSLRegister &reg = Registers[i];

    if(reg.GetNodeId() >= header.Nodes || 
      Nodes[reg.GetNodeId()].GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER ||
        Nodes[reg.GetNodeId()].GetLocalId() != i
    )
      throw std::invalid_argument("Register " + std::to_string(i) + " points to an invalid nodeId");

    if (reg.GetType() > D3D_SIT_UAV_FEEDBACKTEXTURE ||
        reg.GetReturnType() > D3D_RETURN_TYPE_CONTINUED ||
        reg.GetDimension() > D3D_SRV_DIMENSION_BUFFEREX || !reg.GetBindCount() ||
        (reg.GetArrayId() != uint32_t(-1) && reg.GetArrayId() >= header.Arrays) ||
        (reg.GetArrayId() != uint32_t(-1) && reg.GetBindCount() <= 1))
      throw std::invalid_argument(
          "Register " + std::to_string(i) +
          " invalid type, returnType, bindCount, array or dimension");
    
    D3D_CBUFFER_TYPE bufferType = GetBufferType(reg.GetType());

    if(bufferType != D3D_CT_INTERFACE_POINTERS) {

      if (reg.GetBufferId() >= header.Buffers ||
          Buffers[reg.GetBufferId()].NodeId != reg.GetNodeId() ||
          Buffers[reg.GetBufferId()].Type != bufferType)
          throw std::invalid_argument("Register " + std::to_string(i) +
                                      " invalid buffer referenced by register");
    }
  }

  for (uint32_t i = 0; i < header.Functions; ++i) {

    const DxcHLSLFunction &func = Functions[i];

    if (func.GetNodeId() >= header.Nodes ||
        Nodes[func.GetNodeId()].GetNodeType() !=
            D3D12_HLSL_NODE_TYPE_FUNCTION ||
        Nodes[func.GetNodeId()].GetLocalId() != i)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " points to an invalid nodeId");

    uint32_t paramCount = func.GetNumParameters() + func.HasReturn();

    if (Nodes[func.GetNodeId()].GetChildCount() < paramCount)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " is missing parameters and/or return");

    for (uint32_t j = 0; j < paramCount; ++j)
      if (Nodes[func.GetNodeId() + 1 + j].GetParentId() != func.GetNodeId() ||
          Nodes[func.GetNodeId() + 1 + j].GetNodeType() !=
              D3D12_HLSL_NODE_TYPE_PARAMETER)
          throw std::invalid_argument(
              "Function " + std::to_string(i) +
              " is missing valid parameters and/or return");
  }

  for(uint32_t i = 0; i < header.Enums; ++i) {
    
    const DxcHLSLEnumDesc &enm = Enums[i];

    if (enm.NodeId >= header.Nodes ||
        Nodes[enm.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM ||
        Nodes[enm.NodeId].GetLocalId() != i)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " points to an invalid nodeId");

    if (enm.Type < D3D12_HLSL_ENUM_TYPE_START ||
        enm.Type > D3D12_HLSL_ENUM_TYPE_END)
      throw std::invalid_argument("Enum " + std::to_string(i) +
                                  " has an invalid type");

    const DxcHLSLNode &node = Nodes[enm.NodeId];

    if (!node.IsFwdDeclare() && !node.GetChildCount())
      throw std::invalid_argument("Enum " + std::to_string(i) +
                                  " has no values!");

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {
    
        const DxcHLSLNode &child = Nodes[enm.NodeId + 1 + j];

        if (child.GetChildCount() != 0 ||
            child.GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM_VALUE)
          throw std::invalid_argument("Enum " + std::to_string(i) +
                                      " has an invalid enum value");
    }
  }

  for(uint32_t i = 0; i < header.EnumValues; ++i) {
    
    const DxcHLSLEnumValue &enumVal = EnumValues[i];

    if (enumVal.NodeId >= header.Nodes ||
        Nodes[enumVal.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_ENUM_VALUE ||
        Nodes[enumVal.NodeId].GetLocalId() != i ||
        Nodes[Nodes[enumVal.NodeId].GetParentId()].GetNodeType() !=
            D3D12_HLSL_NODE_TYPE_ENUM)
      throw std::invalid_argument("Enum " + std::to_string(i) +
                                  " points to an invalid nodeId");
  }

  for (uint32_t i = 0; i < header.Arrays; ++i) {

    const DxcHLSLArray &arr = Arrays[i];

    if (arr.ArrayElem() <= 1 || arr.ArrayElem() > 32 ||
        arr.ArrayStart() + arr.ArrayElem() > header.ArraySizes)
      throw std::invalid_argument("Array " + std::to_string(i) +
                                  " points to an invalid array element");
  }

  for (uint32_t i = 0; i < header.Annotations; ++i)
    if (Annotations[i].GetStringNonDebug() >= header.StringsNonDebug)
      throw std::invalid_argument("Annotation " + std::to_string(i) +
                                  " points to an invalid string");

  for (uint32_t i = 0; i < header.Buffers; ++i) {

    const DxcHLSLBuffer &buf = Buffers[i];

    if (buf.NodeId >= header.Nodes ||
        Nodes[buf.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER ||
        Nodes[buf.NodeId].GetLocalId() >= header.Registers ||
        Registers[Nodes[buf.NodeId].GetLocalId()].GetBufferId() != i)
      throw std::invalid_argument("Buffer " + std::to_string(i) +
                                  " points to an invalid nodeId");

    const DxcHLSLNode &node = Nodes[buf.NodeId];

    if (!node.GetChildCount())
      throw std::invalid_argument("Buffer " + std::to_string(i) +
                                  " requires at least one Variable child");

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const DxcHLSLNode &child = Nodes[buf.NodeId + 1 + j];

      if (child.GetChildCount() != 0 ||
          child.GetNodeType() != D3D12_HLSL_NODE_TYPE_VARIABLE)
          throw std::invalid_argument("Buffer " + std::to_string(i) +
                                      " has to have only Variable child nodes");
    }
  }

  for (uint32_t i = 0; i < header.Members; ++i) {

    if (MemberTypeIds[i] >= header.Types)
      throw std::invalid_argument("Member " + std::to_string(i) +
                                  " points to an invalid type");

    if (hasSymbolInfo && MemberNameIds[i] >= header.Strings)
      throw std::invalid_argument("Member " + std::to_string(i) +
                                  " points to an invalid string");
  }

  for (uint32_t i = 0; i < header.TypeListCount; ++i)
    if (TypeList[i] >= header.Types)
      throw std::invalid_argument("Type list index " + std::to_string(i) +
                                  " points to an invalid type");

  for (uint32_t i = 0; i < header.Parameters; ++i) {

    const DxcHLSLParameter &param = Parameters[i];

    if (param.NodeId >= header.Nodes ||
        Nodes[param.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_PARAMETER ||
        Nodes[param.NodeId].GetLocalId() != i || param.TypeId >= header.Types)
      throw std::invalid_argument("Parameter " + std::to_string(i) +
                                  " points to an invalid nodeId");

    if (param.Flags > 3 ||
        param.InterpolationMode > D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE)
      throw std::invalid_argument("Parameter " + std::to_string(i) +
                                  " has invalid data");
  }

  for (uint32_t nodeId : validateChildren) {

    const DxcHLSLNode &node = Nodes[nodeId];

    // If/Then/Scope children could only be
    // struct/union/interface/if/variable/typedef/enum

    for (uint32_t j = 0; j < node.GetChildCount(); ++j) {

      const DxcHLSLNode &childNode = Nodes[nodeId + 1 + j];

      switch (childNode.GetNodeType()) {
      case D3D12_HLSL_NODE_TYPE_VARIABLE:
      case D3D12_HLSL_NODE_TYPE_IF:
      case D3D12_HLSL_NODE_TYPE_STRUCT:
      case D3D12_HLSL_NODE_TYPE_UNION:
      case D3D12_HLSL_NODE_TYPE_INTERFACE:
      case D3D12_HLSL_NODE_TYPE_TYPEDEF:
      case D3D12_HLSL_NODE_TYPE_ENUM:
      case D3D12_HLSL_NODE_TYPE_SCOPE:
      case D3D12_HLSL_NODE_TYPE_DO:
      case D3D12_HLSL_NODE_TYPE_FOR:
      case D3D12_HLSL_NODE_TYPE_WHILE:
      case D3D12_HLSL_NODE_TYPE_SWITCH:
        break;
      default:
        throw std::invalid_argument(
            "Node " + std::to_string(nodeId) +
            " has if/then/scope with children of invalid type");
      }

      j += childNode.GetChildCount();
    }
  }

  for (uint32_t i = 0; i < header.Statements; ++i) {

    const DxcHLSLStatement &Stmt = Statements[i];

    if (Stmt.GetNodeId() >= header.Nodes ||
        Nodes[Stmt.GetNodeId()].GetLocalId() != i)
      throw std::invalid_argument("Statement " + std::to_string(i) +
                                  " points to an invalid nodeId");

    bool condVar = Stmt.HasConditionVar();
    uint32_t minParamCount = Stmt.GetNodeCount() + condVar;
    const DxcHLSLNode &node = Nodes[Stmt.GetNodeId()];

    if (node.GetChildCount() < minParamCount)
      throw std::invalid_argument("Statement " + std::to_string(i) +
                                  " didn't have required child nodes");

    if (condVar && Nodes[Stmt.GetNodeId() + 1].GetNodeType() !=
                       D3D12_HLSL_NODE_TYPE_VARIABLE)
      throw std::invalid_argument(
          "Statement " + std::to_string(i) +
          " has condition variable but first child is not a variable");

    switch (Nodes[Stmt.GetNodeId()].GetNodeType()) {
    case D3D12_HLSL_NODE_TYPE_IF:
    case D3D12_HLSL_NODE_TYPE_WHILE:
    case D3D12_HLSL_NODE_TYPE_FOR:
    case D3D12_HLSL_NODE_TYPE_SWITCH:
      break;
    default:
      throw std::invalid_argument("Statement " + std::to_string(i) +
                                  " has invalid node type");
    }
  }
  
  for (uint32_t i = 0; i < header.Types; ++i) {

    const DxcHLSLType &type = Types[i];

    if (hasSymbolInfo && (TypeSymbols[i].DisplayNameId >= header.Strings ||
                          TypeSymbols[i].UnderlyingNameId >= header.Strings))
      throw std::invalid_argument("Type " + std::to_string(i) +
                                  " points to an invalid string");

    if (hasSymbolInfo && (TypeSymbols[i].DisplayArray.ElementsOrArrayId >> 31 &&
                          (TypeSymbols[i].DisplayArray.ElementsOrArrayId << 1 >>
                           1) >= header.Arrays))
      throw std::invalid_argument("Type " + std::to_string(i) +
                                  " points to an invalid string");

    if ((type.GetBaseClass() != uint32_t(-1) &&
         type.GetBaseClass() >= header.Types) ||
        type.GetMemberStart() + type.GetMemberCount() > header.Members ||
        type.GetInterfaceStart() + type.GetInterfaceCount() >
            header.TypeListCount ||
        (type.GetUnderlyingArray().ElementsOrArrayId >> 31 &&
         (type.GetUnderlyingArray().ElementsOrArrayId << 1 >> 1) >=
             header.Arrays))
      throw std::invalid_argument(
          "Type " + std::to_string(i) +
          " points to an invalid string, array, base class or member");

    switch (type.GetClass()) {

    case D3D_SVC_SCALAR:

      if (type.GetColumns() != 1)
          throw std::invalid_argument("Type (scalar) " + std::to_string(i) +
                                      " should have columns == 1");

      [[fallthrough]];

    case D3D_SVC_VECTOR:

      if (type.GetRows() != 1)
          throw std::invalid_argument("Type (scalar/vector) " +
                                      std::to_string(i) +
                                      " should have rows == 1");

      [[fallthrough]];

    case D3D_SVC_MATRIX_ROWS:
    case D3D_SVC_MATRIX_COLUMNS:

        if (!type.GetRows() || !type.GetColumns() || type.GetRows() > 128 ||
          type.GetColumns() > 128)
          throw std::invalid_argument("Type (scalar/vector/matrix) " +
                                      std::to_string(i) +
                                      " has invalid rows or columns");
        
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
          throw std::invalid_argument("Type (scalar/matrix/vector) " +
                                      std::to_string(i) +
                                      " is of invalid type");
        }

        break;

    case D3D_SVC_STRUCT:

        if (!type.GetMemberCount())
          throw std::invalid_argument("Type (struct) " + std::to_string(i) +
                                      " is missing children");

        [[fallthrough]];

    case D3D_SVC_INTERFACE_CLASS:

        if (type.GetType())
          throw std::invalid_argument("Type (struct) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

        if (type.GetRows() || type.GetColumns())
          throw std::invalid_argument("Type (struct) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

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
          throw std::invalid_argument("Type (object) " + std::to_string(i) +
                                      " is of invalid type");
        }

        if (type.GetRows() || type.GetColumns())
          throw std::invalid_argument("Type (object) " +
                                      std::to_string(i) +
                                      " shouldn't have rows or columns");

      break;

    default:
      throw std::invalid_argument("Type " + std::to_string(i) +
                                  " has an invalid class");
    }
  }

  //Validate fwd & backwards declares

   for (uint32_t i = 0; i < header.Nodes; ++i) {

    const DxcHLSLNode &node = Nodes[i];

    if (node.IsFwdBckDefined()) {

      uint32_t fwdBack = node.GetFwdBck();

      if (Nodes[fwdBack].GetNodeType() != node.GetNodeType())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd/bck declare) points to element that of incompatible type");

      if (hasSymbolInfo && NodeSymbols[fwdBack].GetNameId() != NodeSymbols[i].GetNameId())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd/bck declare) have mismatching name");

      if (node.IsFwdDeclare()) {

        if (fwdBack <= i || fwdBack >= header.Nodes)
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd declare) points to invalid element");

        if (Nodes[fwdBack].IsFwdDeclare())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd declare) points to element that is also a fwd declare");

        if (node.GetChildCount() || node.GetAnnotationCount())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd declare) points to element with invalid child count, "
              "or annotationCount");
      }

      else {

        if (fwdBack >= i)
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (bck declare) points to invalid element");

        if (!Nodes[fwdBack].IsFwdDeclare())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (bck declare) points to element that is not a fwd declare");
      }
    }
  }

  //Finalize

  if (MakeNameLookupTable)
    GenerateNameLookupTable();
}

void DxcHLSLReflectionData::Printf() const { RecursePrint(*this, 0, 0, 0); }

} // namespace hlsl
