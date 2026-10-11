///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflectionJson.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxcReflection/DxcReflectionContainer.h"
#include <functional>
#include <sstream>

namespace hlsl {

struct JsonWriter {

  std::stringstream ss;
  uint16_t indent = 0;
  uint16_t countCommaStack = 0;
  uint32_t needCommaStack[3] = {0, 0, 0};

  void Indent() { ss << std::string(indent, '\t'); }
  void NewLine() { ss << "\n"; }

  void StartCommaStack() {
    ++countCommaStack;
    assert(countCommaStack < 96 && "countCommaStack out of bounds");
    needCommaStack[countCommaStack / 32] &= ~(1u << (countCommaStack & 31));
  }

  void SetComma() {
    needCommaStack[countCommaStack / 32] |= 1u << (countCommaStack & 31);
  }

  void EndCommaStack() {
    --countCommaStack;
    SetComma();
  }

  bool NeedsComma() {
    return (needCommaStack[countCommaStack / 32] >> (countCommaStack & 31)) & 1;
  }

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
    ArrayScope _(*this, Name);
    Body();
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
                              "IfRoot",
                              "Scope",
                              "Do",
                              "Switch",
                              "While",
                              "For",
                              "GroupsharedVariable",
                              "Case",
                              "Default",
                              "Using",
                              "IfFirst",
                              "ElseIf",
                              "Else"};

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

static std::string GetBuiltinTypeName(const ReflectionData &Refl,
                                      const ReflectionVariableType &Type) {

  std::string type = "";

  if (Type.GetClass() != D3D_SVC_STRUCT &&
      Type.GetClass() != D3D_SVC_INTERFACE_CLASS) {

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

static void FillArraySizes(const ReflectionData &Reflection,
                           ReflectionArrayOrElements Elements,
                           std::vector<uint32_t> &Array) {

  if (!Elements.IsArray())
    return;

  if (Elements.Is1DArray()) {
    Array.push_back(Elements.Get1DElements());
    return;
  }

  const ReflectionArray &arr =
      Reflection.Arrays[Elements.GetMultiDimensionalArrayId()];

  for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
    Array.push_back(Reflection.ArraySizes[arr.ArrayStart() + i]);
}

struct ReflectionPrintSettings {
  bool HumanReadable;
  bool HideFileInfo;
};

static void PrintSymbol(JsonWriter &Json, const ReflectionData &Reflection,
                        const ReflectionNodeSymbol &Sym,
                        const ReflectionPrintSettings &Settings, bool MuteName,
                        bool ShowOnlyName = false) {

  if (Sym.GetNameId() && !MuteName) {

    Json.StringField("Name", Reflection.Strings[Sym.GetNameId()]);

    if (!Settings.HumanReadable)
      Json.UIntField("NameId", Sym.GetNameId());
  }

  if (Settings.HideFileInfo || ShowOnlyName)
    return;

  if (Sym.HasFileSource()) {

    Json.StringField(
        "Source",
        Reflection.Strings[Reflection.Sources[Sym.GetFileSourceId()]]);

    if (!Settings.HumanReadable)
      Json.UIntField("SourceId", Sym.GetFileSourceId());

    Json.UIntField("LineId", Sym.GetSourceLineStart());

    if (Sym.GetSourceLineCount() > 1)
      Json.UIntField("LineCount", Sym.GetSourceLineCount());

    Json.UIntField("ColumnStart", Sym.GetSourceColumnStart());
    Json.UIntField("ColumnEnd", Sym.GetSourceColumnEnd());
  }
}

static void PrintInterpolationMode(JsonWriter &Json,
                                   D3D_INTERPOLATION_MODE Interp) {

  static const char *interpolationModes[] = {"Undefined",
                                             "Constant",
                                             "Linear",
                                             "LinearCentroid",
                                             "LinearNoperspective",
                                             "LinearNoperspectiveCentroid",
                                             "LinearSample",
                                             "LinearNoperspectiveSample"};
  if (Interp)
    Json.StringField("Interpolation", interpolationModes[Interp]);
}

// Verbose and all members are slightly different;
// Verbose will still print fields even if they aren't relevant,
//  while all members will not silence important info but that might not matter
//  for human readability
static void PrintNode(JsonWriter &Json, const ReflectionData &Reflection,
                      uint32_t NodeId,
                      const ReflectionPrintSettings &Settings) {

  const ReflectionNode &node = Reflection.Nodes[NodeId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!Settings.HumanReadable || !hasSymbols)
    Json.UIntField("NodeId", NodeId);

  Json.StringField("NodeType", NodeTypeToString(node.GetNodeType()));

  if (!Settings.HumanReadable) {
    Json.UIntField("NodeTypeId", node.GetNodeType());
    Json.UIntField("LocalId", node.GetLocalId());
    Json.IntField("ParentId", node.GetParentId() == uint16_t(-1)
                                  ? -1
                                  : int64_t(node.GetParentId()));
  }

  if (node.GetChildCount() && !Settings.HumanReadable) {
    Json.UIntField("ChildCount", node.GetChildCount());
    Json.UIntField("ChildStart", NodeId + 1);
  }

  if (node.GetSemanticId() != uint32_t(-1)) {

    Json.StringField("Semantic",
                     Reflection.StringsNonDebug[node.GetSemanticId()]);

    if (!Settings.HumanReadable)
      Json.UIntField("SemanticId", node.GetSemanticId());
  }

  PrintInterpolationMode(Json, node.GetInterpolationMode());

  if (node.GetAnnotationCount()) {

    if (!Settings.HumanReadable) {
      Json.UIntField("AnnotationStart", node.GetAnnotationStart());
      Json.UIntField("AnnotationCount", node.GetAnnotationCount());
    }

    Json.Array("Annotations", [&Reflection, &Json, node] {
      for (uint32_t i = 0; i < node.GetAnnotationCount(); ++i) {

        const ReflectionAnnotation &annot =
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
  }

  if ((node.IsFwdBckDefined() || node.IsFwdDeclare()) &&
      !Settings.HumanReadable) {
    Json.BoolField("IsFwdDeclare", node.IsFwdDeclare());
    Json.IntField("FwdBack",
                  !node.IsFwdBckDefined() ? -1 : int64_t(node.GetFwdBck()));
  }

  if (hasSymbols && !Settings.HideFileInfo)
    Json.Object("Symbol", [&Reflection, &Json, NodeId, &Settings] {
      const ReflectionNodeSymbol &sym = Reflection.NodeSymbols[NodeId];
      PrintSymbol(Json, Reflection, sym, Settings, true);
    });
}

static void PrintRegister(JsonWriter &Json, const ReflectionData &Reflection,
                          uint32_t RegisterId,
                          const ReflectionPrintSettings &Settings) {

  const ReflectionShaderResource &reg = Reflection.Registers[RegisterId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!Settings.HumanReadable || !hasSymbols) {
    Json.UIntField("RegisterId", RegisterId);
    Json.UIntField("NodeId", reg.GetNodeId());
  }

  if (!Settings.HumanReadable && hasSymbols)
    Json.StringField(
        "Name",
        Reflection
            .Strings[Reflection.NodeSymbols[reg.GetNodeId()].GetNameId()]);

  Json.StringField("RegisterType", RegisterTypeToString(reg.GetType()));

  if (reg.GetDimension() != D3D_SRV_DIMENSION_UNKNOWN)
    Json.StringField("Dimension", DimensionTypeToString(reg.GetDimension()));

  if (reg.GetReturnType())
    Json.StringField("ReturnType", ReturnTypeToString(reg.GetReturnType()));

  if (reg.GetBindCount() > 1)
    Json.UIntField("BindCount", reg.GetBindCount());

  if (reg.GetArrayId() != uint32_t(-1)) {

    if (!Settings.HumanReadable)
      Json.UIntField("ArrayId", reg.GetArrayId());

    Json.Array("ArraySize", [&Reflection, &reg, &Json]() {
      const ReflectionArray &arr = Reflection.Arrays[reg.GetArrayId()];

      for (uint32_t i = 0; i < uint32_t(arr.ArrayElem()); ++i)
        Json.Value(uint64_t(Reflection.ArraySizes[arr.ArrayStart() + i]));
    });
  }

  bool printBufferId = true;

  switch (reg.GetType()) {
  case D3D_SIT_TEXTURE:
  case D3D_SIT_SAMPLER:
  case D3D_SIT_UAV_RWTYPED:
  case D3D_SIT_RTACCELERATIONSTRUCTURE:
  case D3D_SIT_UAV_FEEDBACKTEXTURE:
  case D3D_SIT_UAV_RWBYTEADDRESS:
  case D3D_SIT_BYTEADDRESS:
    printBufferId = false;
    break;
  }

  if (printBufferId && !Settings.HumanReadable)
    Json.UIntField("BufferId", reg.GetBufferId());

  if (reg.GetFlags())
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

static void PrintTypeName(const ReflectionData &Reflection, uint32_t TypeId,
                          bool HasSymbols,
                          const ReflectionPrintSettings &Settings,
                          JsonWriter &Json,
                          const char *NameForTypeName = "Name",
                          bool MuteArgs = false) {

  if (!Settings.HumanReadable || !HasSymbols)
    Json.UIntField("TypeId", TypeId);

  std::string name;
  std::vector<uint32_t> arraySizes;

  std::string underlyingName;
  std::vector<uint32_t> underlyingArraySizes;

  const ReflectionVariableType &type = Reflection.Types[TypeId];

  if (!HasSymbols) {
    name = GetBuiltinTypeName(Reflection, type);
    FillArraySizes(Reflection, type.GetUnderlyingArray(), arraySizes);
  }

  else {

    const ReflectionVariableTypeSymbol &symbol = Reflection.TypeSymbols[TypeId];

    name = Reflection.Strings[symbol.DisplayNameId];
    underlyingName = Reflection.Strings[symbol.UnderlyingNameId];

    FillArraySizes(Reflection, symbol.DisplayArray, arraySizes);
    FillArraySizes(Reflection, type.GetUnderlyingArray(), underlyingArraySizes);
  }

  if (name.size())
    Json.StringField(NameForTypeName, name);

  if (type.GetClass() == D3D_SVC_OBJECT &&
      type.GetType() != D3D_SVT_BYTEADDRESS_BUFFER &&
      type.GetType() != D3D_SVT_RWBYTEADDRESS_BUFFER &&
      type.GetMemberCount() == 1 && !MuteArgs) {

    uint32_t innerTypeId = Reflection.MemberTypeIds[type.GetMemberStart()];

    Json.Array(
        "Args", [&Json, &Reflection, innerTypeId, HasSymbols, &Settings]() {
          JsonWriter::ObjectScope scope(Json);
          PrintTypeName(Reflection, innerTypeId, HasSymbols, Settings, Json);
        });
  }

  if (arraySizes.size())
    Json.Array("ArraySize", [&arraySizes, &Json]() {
      for (uint32_t i : arraySizes)
        Json.Value(uint64_t(i));
    });

  if (underlyingName.size() && underlyingName != name)
    Json.StringField("UnderlyingName", underlyingName);

  if (underlyingArraySizes.size() && underlyingArraySizes != arraySizes)
    Json.Array("UnderlyingArraySize", [&underlyingArraySizes, &Json]() {
      for (uint32_t i : underlyingArraySizes)
        Json.Value(uint64_t(i));
    });
}

static void PrintType(const ReflectionData &Reflection, uint32_t TypeId,
                      bool HasSymbols, const ReflectionPrintSettings &Settings,
                      JsonWriter &Json, bool Recursive,
                      const char *NameForTypeName = "Name") {

  const ReflectionVariableType &type = Reflection.Types[TypeId];

  PrintTypeName(Reflection, TypeId, HasSymbols, Settings, Json, NameForTypeName,
                true);

  if (type.GetBaseClass() != uint32_t(-1))
    Json.Object("BaseClass",
                [&Reflection, &Json, &type, HasSymbols, Settings, Recursive]() {
                  if (Recursive)
                    PrintType(Reflection, type.GetBaseClass(), HasSymbols,
                              Settings, Json, true, "TypeName");

                  else
                    PrintTypeName(Reflection, type.GetBaseClass(), HasSymbols,
                                  Settings, Json, "TypeName");
                });

  if (type.GetInterfaceCount())
    Json.Array(
        "Interfaces", [&Reflection, &Json, &type, HasSymbols, Settings]() {
          for (uint32_t i = 0; i < uint32_t(type.GetInterfaceCount()); ++i) {
            uint32_t interfaceId = type.GetInterfaceStart() + i;
            JsonWriter::ObjectScope nodeRoot(Json);
            PrintTypeName(Reflection, Reflection.TypeList[interfaceId],
                          HasSymbols, Settings, Json);
          }
        });

  if (type.GetMemberCount())
    Json.Array("Members", [&Reflection, &Json, &type, HasSymbols, Settings,
                           Recursive]() {
      for (uint32_t i = 0; i < uint32_t(type.GetMemberCount()); ++i) {

        uint32_t memberId = type.GetMemberStart() + i;
        JsonWriter::ObjectScope nodeRoot(Json);

        if (HasSymbols) {
          Json.StringField(
              "Name", Reflection.Strings[Reflection.MemberNameIds[memberId]]);

          if (!Settings.HumanReadable)
            Json.UIntField("NameId", Reflection.MemberNameIds[memberId]);
        }

        if (Recursive)
          PrintType(Reflection, Reflection.MemberTypeIds[memberId], HasSymbols,
                    Settings, Json, true, "TypeName");

        else
          PrintTypeName(Reflection, Reflection.MemberTypeIds[memberId],
                        HasSymbols, Settings, Json, "TypeName");
      }
    });
}

static void PrintParameter(const ReflectionData &Reflection, uint32_t TypeId,
                           bool HasSymbols, JsonWriter &Json,
                           uint32_t SemanticId,
                           D3D_INTERPOLATION_MODE InterpMode, uint8_t Flags,
                           const ReflectionPrintSettings &Settings) {

  PrintTypeName(Reflection, TypeId, HasSymbols, Settings, Json, "TypeName");

  if (SemanticId != uint32_t(-1)) {

    Json.StringField("Semantic", Reflection.StringsNonDebug[SemanticId]);

    if (!Settings.HumanReadable)
      Json.UIntField("SemanticId", SemanticId);
  }

  if ((Flags & (D3D_PF_IN | D3D_PF_OUT)) == (D3D_PF_IN | D3D_PF_OUT))
    Json.StringField("Access", "inout");

  else if (Flags & D3D_PF_IN)
    Json.StringField("Access", "in");

  else if (Flags & D3D_PF_OUT)
    Json.StringField("Access", "out");

  PrintInterpolationMode(Json, InterpMode);
}

static void PrintFunction(JsonWriter &Json, const ReflectionData &Reflection,
                          uint32_t FunctionId,
                          const ReflectionPrintSettings &Settings) {

  const ReflectionFunction &func = Reflection.Functions[FunctionId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!Settings.HumanReadable || !hasSymbols) {
    Json.UIntField("FunctionId", FunctionId);
    Json.UIntField("NodeId", func.GetNodeId());
  }

  if (!Settings.HumanReadable && hasSymbols)
    Json.StringField(
        "Name",
        Reflection
            .Strings[Reflection.NodeSymbols[func.GetNodeId()].GetNameId()]);

  if (!func.HasDefinition() || !Settings.HumanReadable)
    Json.BoolField("HasDefinition", func.HasDefinition());

  Json.Object("Params", [&Reflection, &func, &Json, hasSymbols, &Settings]() {
    for (uint32_t i = 0; i < uint32_t(func.GetNumParameters()); ++i) {

      uint32_t nodeId = func.GetNodeId() + 1 + i;
      const ReflectionNode &node = Reflection.Nodes[nodeId];
      uint32_t localId = node.GetLocalId();

      const ReflectionFunctionParameter &param = Reflection.Parameters[localId];
      std::string paramName =
          hasSymbols
              ? Reflection.Strings[Reflection.NodeSymbols[nodeId].GetNameId()]
              : std::to_string(i);

      Json.Object(paramName.c_str(), [&Reflection, &func, &Json, hasSymbols,
                                      &param, &node, &Settings, nodeId]() {
        if (hasSymbols && !Settings.HideFileInfo) {

          const ReflectionNodeSymbol &sym = Reflection.NodeSymbols[nodeId];

          Json.Object("Symbol", [&Json, &Reflection, &sym, &Settings]() {
            PrintSymbol(Json, Reflection, sym, Settings, true);
          });
        }

        PrintParameter(Reflection, param.TypeId, hasSymbols, Json,
                       node.GetSemanticId(), node.GetInterpolationMode(),
                       param.Flags, Settings);
      });
    }
  });

  if (!func.HasReturn())
    Json.Object("ReturnType",
                [&Json]() { Json.StringField("TypeName", "void"); });

  else {

    const ReflectionNode &node =
        Reflection.Nodes[func.GetNodeId() + 1 + func.GetNumParameters()];
    const ReflectionFunctionParameter &param =
        Reflection.Parameters[node.GetLocalId()];

    Json.Object("ReturnType", [&Reflection, &func, &Json, hasSymbols, &param,
                               &node, &Settings]() {
      PrintParameter(Reflection, param.TypeId, hasSymbols, Json,
                     node.GetSemanticId(), node.GetInterpolationMode(),
                     param.Flags, Settings);
    });
  }
}

static void PrintValue(JsonWriter &Json, D3D12_HLSL_ENUM_TYPE type, uint64_t v,
                       const char *Name = "Value") {

  switch (type) {

  case D3D12_HLSL_ENUM_TYPE_INT:
    Json.IntField("Value", int32_t(uint32_t(v)));
    break;

  case D3D12_HLSL_ENUM_TYPE_INT64_T:
    Json.IntField("Value", int64_t(v));
    break;

  case D3D12_HLSL_ENUM_TYPE_INT16_T:
    Json.IntField("Value", int16_t(uint16_t(v)));
    break;

  default:
    Json.UIntField("Value", v);
    break;
  }
}

static void PrintEnumValue(JsonWriter &Json, const ReflectionData &Reflection,
                           uint32_t NodeId,
                           const ReflectionPrintSettings &Settings) {

  const ReflectionNode &child = Reflection.Nodes[NodeId];

  const ReflectionEnumValue &val = Reflection.EnumValues[child.GetLocalId()];

  const ReflectionNode &parent = Reflection.Nodes[child.GetParentId()];
  const ReflectionEnumeration &enm = Reflection.Enums[parent.GetLocalId()];

  PrintValue(Json, enm.Type, val.Value);

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (hasSymbols) {

    const ReflectionNodeSymbol &sym = Reflection.NodeSymbols[NodeId];

    Json.Object("Symbol", [&Json, &Reflection, &sym, &Settings]() {
      PrintSymbol(Json, Reflection, sym, Settings, false);
    });
  }
}

static void PrintEnum(JsonWriter &Json, const ReflectionData &Reflection,
                      uint32_t EnumId,
                      const ReflectionPrintSettings &Settings) {

  const ReflectionEnumeration &enm = Reflection.Enums[EnumId];
  const ReflectionNode &node = Reflection.Nodes[enm.NodeId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!Settings.HumanReadable || !hasSymbols) {
    Json.UIntField("EnumId", EnumId);
    Json.UIntField("NodeId", enm.NodeId);
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection.Strings[Reflection.NodeSymbols[enm.NodeId].GetNameId()]);

  Json.StringField("EnumType", EnumTypeToString(enm.Type));

  Json.Array("Values",
             [&Json, &node, &enm, hasSymbols, &Reflection, &Settings]() {
               for (uint32_t i = 0; i < node.GetChildCount(); ++i) {

                 uint32_t childId = enm.NodeId + 1 + i;

                 JsonWriter::ObjectScope valueRoot(Json);

                 if (!hasSymbols || !Settings.HumanReadable)
                   Json.UIntField("ValueId", i);

                 PrintEnumValue(Json, Reflection, childId, Settings);
               }
             });
}

static void PrintAnnotation(JsonWriter &Json, const ReflectionData &Reflection,
                            const ReflectionAnnotation &Annot) {
  Json.StringField("Contents",
                   Reflection.StringsNonDebug[Annot.GetStringNonDebug()]);
  Json.StringField("Type", Annot.GetIsBuiltin() ? "Builtin" : "User");
}

static uint32_t PrintBufferMember(const ReflectionData &Reflection,
                                  uint32_t NodeId, uint32_t ChildId,
                                  bool HasSymbols,
                                  const ReflectionPrintSettings &Settings,
                                  JsonWriter &Json) {

  const ReflectionNode &node = Reflection.Nodes[NodeId];

  JsonWriter::ObjectScope root(Json);

  if (!Settings.HumanReadable)
    Json.UIntField("NodeId", NodeId);

  if (!HasSymbols || !Settings.HumanReadable)
    Json.UIntField("ChildId", ChildId);

  if (HasSymbols)
    Json.StringField(
        "Name", Reflection.Strings[Reflection.NodeSymbols[NodeId].GetNameId()]);

  PrintType(Reflection, node.GetLocalId(), HasSymbols, Settings, Json, true,
            "TypeName");

  return node.GetChildCount();
}

static void PrintBuffer(const ReflectionData &Reflection, uint32_t BufferId,
                        bool HasSymbols,
                        const ReflectionPrintSettings &Settings,
                        JsonWriter &Json) {

  JsonWriter::ObjectScope nodeRoot(Json);
  const ReflectionShaderBuffer &buf = Reflection.Buffers[BufferId];
  const ReflectionNode &node = Reflection.Nodes[buf.NodeId];

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  if (!Settings.HumanReadable || !hasSymbols) {
    Json.UIntField("BufferId", BufferId);
    Json.UIntField("NodeId", buf.NodeId);
  }

  if (hasSymbols)
    Json.StringField(
        "Name",
        Reflection.Strings[Reflection.NodeSymbols[buf.NodeId].GetNameId()]);

  Json.StringField("Type", BufferTypeToString(buf.Type));

  if (node.GetChildCount())
    Json.Array("Children",
               [&node, &Reflection, &buf, &Json, HasSymbols, &Settings]() {
                 for (uint32_t i = 0, j = 0; i < node.GetChildCount(); ++i, ++j)
                   i += PrintBufferMember(Reflection, buf.NodeId + 1 + i, j,
                                          HasSymbols, Settings, Json);
               });
}

static void PrintStatement(const ReflectionData &Reflection,
                           const ReflectionScopeStmt &Stmt, JsonWriter &Json) {

  const ReflectionNode &node = Reflection.Nodes[Stmt.GetNodeId()];

  uint32_t nodesA = Stmt.GetNodeCount();
  uint32_t nodesB = node.GetChildCount() - nodesA - Stmt.HasConditionVar();

  if (Stmt.HasConditionVar())
    Json.BoolField("HasConditionVar", Stmt.HasConditionVar());

  if (nodesA)
    Json.UIntField("Init", nodesA);

  if (nodesB)
    Json.UIntField("Body", nodesB);
}

static void PrintIfSwitchStatement(const ReflectionData &Reflection,
                                   const ReflectionIfSwitchStmt &Stmt,
                                   JsonWriter &Json) {

  if (Stmt.HasConditionVar())
    Json.BoolField("HasConditionVar", Stmt.HasConditionVar());

  if (Stmt.HasElseOrDefault())
    Json.BoolField("HasElseOrDefault", Stmt.HasElseOrDefault());
}

static void PrintBranchStatement(const ReflectionData &Reflection,
                                 const ReflectionBranchStmt &Stmt,
                                 JsonWriter &Json) {

  if (Stmt.HasConditionVar())
    Json.BoolField("HasConditionVar", Stmt.HasConditionVar());

  if (!Stmt.IsComplexCase()) {
    Json.StringField("ValueType", EnumTypeToString(Stmt.GetValueType()));
    PrintValue(Json, Stmt.GetValueType(), Stmt.GetValue());
  }

  else
    Json.BoolField("IsComplexCase", Stmt.IsComplexCase());
}

uint32_t PrintNodeRecursive(const ReflectionData &Reflection, uint32_t NodeId,
                            JsonWriter &Json,
                            const ReflectionPrintSettings &Settings);

void PrintChildren(const ReflectionData &Data, JsonWriter &Json,
                   const char *ObjectName, uint32_t Start, uint32_t End,
                   const ReflectionPrintSettings &Settings) {

  if (End > Start)
    Json.Array(ObjectName, [&Data, &Json, Start, End, &Settings]() {
      for (uint32_t i = Start; i < End; ++i) {

        const ReflectionNode &node = Data.Nodes[i];

        if (node.IsFwdBckDefined() && !node.IsFwdDeclare()) {
          i += node.GetChildCount();
          continue;
        }

        JsonWriter::ObjectScope scope(Json);

        // Put Name(Id) into current scope to hide "Symbol" everywhere.

        if (Data.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO)
          PrintSymbol(Json, Data, Data.NodeSymbols[i], Settings, false, true);

        i += PrintNodeRecursive(Data, i, Json, Settings);
      }
    });
}

uint32_t PrintNodeRecursive(const ReflectionData &Reflection, uint32_t NodeId,
                            JsonWriter &Json,
                            const ReflectionPrintSettings &Settings) {

  bool hasSymbols =
      Reflection.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  ReflectionNode node = Reflection.Nodes[NodeId];

  // In case we're a fwd declare, don't change how we walk the tree

  uint32_t nodeChildCountForRet = node.GetChildCount();

  // If this happens, we found the one defining a fwd declare.
  // But this can happen in a different scope than the symbol ends up in.
  // Mute this node.
  // (This should be checked by the caller to avoid empty objects laying around)
  if (node.IsFwdBckDefined() && !node.IsFwdDeclare())
    return node.GetChildCount();

  D3D12_HLSL_NODE_TYPE nodeType = node.GetNodeType();

  if (node.IsFwdDeclare() && node.IsFwdBckDefined()) {
    NodeId = node.GetFwdBck();
    node = Reflection.Nodes[NodeId];
  }

  PrintNode(Json, Reflection, NodeId, Settings);

  uint32_t childrenToSkip = 0;

  bool recurseType = false;
  const char *stmtType = nullptr;

  switch (nodeType) {

  case D3D12_HLSL_NODE_TYPE_FUNCTION:
    Json.Object(
        "Function", [&node, &Reflection, &Json, &Settings, &childrenToSkip]() {
          ReflectionFunction func = Reflection.Functions[node.GetLocalId()];
          PrintFunction(Json, Reflection, node.GetLocalId(), Settings);
          childrenToSkip = func.GetNumParameters() + func.HasReturn();
        });
    break;

  case D3D12_HLSL_NODE_TYPE_REGISTER:
    Json.Object("Register", [&node, &Reflection, &Json, &Settings]() {
      PrintRegister(Json, Reflection, node.GetLocalId(), Settings);
    });
    break;

  case D3D12_HLSL_NODE_TYPE_ENUM:
    Json.Object("Enum",
                [&node, &Reflection, &Json, &Settings, &childrenToSkip]() {
                  PrintEnum(Json, Reflection, node.GetLocalId(), Settings);
                  childrenToSkip = node.GetChildCount();
                });
    break;

  case D3D12_HLSL_NODE_TYPE_STRUCT:
  case D3D12_HLSL_NODE_TYPE_UNION: {

    if (node.IsFwdDeclare())
      break;

    const ReflectionVariableType &type = Reflection.Types[node.GetLocalId()];

    if (type.GetBaseClass() != uint32_t(-1) || type.GetInterfaceCount())
      Json.Object("Class", [&Json, &type, &Reflection, hasSymbols,
                            &Settings]() {
        if (type.GetBaseClass() != uint32_t(-1))
          Json.Object("BaseClass",
                      [&Json, &type, &Reflection, hasSymbols, &Settings]() {
                        PrintTypeName(Reflection, type.GetBaseClass(),
                                      hasSymbols, Settings, Json);
                      });

        if (type.GetInterfaceCount())
          Json.Array("Interfaces", [&Json, &type, &Reflection, hasSymbols,
                                    &Settings]() {
            for (uint32_t i = 0; i < uint32_t(type.GetInterfaceCount()); ++i) {
              uint32_t interfaceId = type.GetInterfaceStart() + i;
              JsonWriter::ObjectScope nodeRoot(Json);
              PrintTypeName(Reflection, Reflection.TypeList[interfaceId],
                            hasSymbols, Settings, Json);
            }
          });
      });

    break;
  }

  case D3D12_HLSL_NODE_TYPE_VARIABLE:
  case D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE:
  case D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE:
    recurseType = true;
    [[fallthrough]];

  case D3D12_HLSL_NODE_TYPE_TYPEDEF:
  case D3D12_HLSL_NODE_TYPE_USING:

    Json.Object("Type", [&node, &Reflection, &Json, &Settings, hasSymbols,
                         recurseType]() {
      PrintType(Reflection, node.GetLocalId(), hasSymbols, Settings, Json,
                recurseType);
    });

    break;

  case D3D12_HLSL_NODE_TYPE_CASE:

    Json.Object(
        "Case", [&node, &Reflection, &Json, &Settings, &childrenToSkip]() {
          const ReflectionBranchStmt &branch =
              Reflection.BranchStatements[node.GetLocalId()];
          if (!branch.IsComplexCase()) {
            Json.StringField("Type", EnumTypeToString(branch.GetValueType()));
            PrintValue(Json, branch.GetValueType(), branch.GetValue());
          }
        });

    break;

  case D3D12_HLSL_NODE_TYPE_IF_FIRST:
  case D3D12_HLSL_NODE_TYPE_ELSE_IF: {

    const ReflectionBranchStmt &stmt = Reflection.BranchStatements[node.GetLocalId()];
    uint32_t start = NodeId + 1;

    if (stmt.HasConditionVar())
      Json.Object("Branch", [NodeId, &Reflection, &Json, &start, &Settings,
                         hasSymbols, &childrenToSkip]() {
        Json.Object("Condition", [NodeId, &Reflection, &Json, &start, &Settings,
                                  hasSymbols, &childrenToSkip]() {
          if (hasSymbols)
            PrintSymbol(Json, Reflection, Reflection.NodeSymbols[start],
                        Settings, false, true);

          start += PrintNodeRecursive(Reflection, start, Json, Settings);
          ++start;

          childrenToSkip = start - NodeId - 1;
        });
      });

    break;
  }

  case D3D12_HLSL_NODE_TYPE_FOR:
    stmtType = "For";
    break;

  case D3D12_HLSL_NODE_TYPE_WHILE:
    stmtType = "While";
    break;
  }

  // While; turns into ("Condition"), ("Body")
  // For; turns into ("Condition"), ("Init"), ("Body")

  if (stmtType) {
    Json.Object(stmtType, [&node, &Reflection, &Json, &Settings, NodeId,
                           &childrenToSkip, nodeType, hasSymbols]() {
      const ReflectionScopeStmt &stmt =
          Reflection.Statements[node.GetLocalId()];

      uint32_t start = NodeId + 1;

      if (stmt.HasConditionVar())
        Json.Object("Condition", [NodeId, &Reflection, &Json, &start, &Settings,
                                  hasSymbols]() {
          if (hasSymbols)
            PrintSymbol(Json, Reflection, Reflection.NodeSymbols[start],
                        Settings, false, true);

          start += PrintNodeRecursive(Reflection, start, Json, Settings);
          ++start;
        });

      uint32_t end = start + stmt.GetNodeCount();

      if (stmt.GetNodeCount())
        PrintChildren(Reflection, Json, "Init", start, end, Settings);

      start = end;
      end = NodeId + 1 + node.GetChildCount();

      PrintChildren(Reflection, Json, "Body", start, end, Settings);

      childrenToSkip = node.GetChildCount();
    });
  }

  // Switch; turns into ("Condition"), ("Case": [])
  // If(Root); is just a container for IfFirst/ElseIf/Else (no need to handle it here)

  else if (nodeType == D3D12_HLSL_NODE_TYPE_SWITCH) {

    const ReflectionIfSwitchStmt &stmt =
        Reflection.IfSwitchStatements[node.GetLocalId()];

    if (stmt.HasConditionVar())
      Json.Object("Switch", [&stmt, &Reflection, &Json, &Settings, NodeId,
                             &childrenToSkip, nodeType, hasSymbols]() {
        uint32_t start = NodeId + 1;

        if (stmt.HasConditionVar())
          Json.Object("Condition", [NodeId, &Reflection, &Json, &start,
                                    &Settings, hasSymbols]() {
            if (hasSymbols)
              PrintSymbol(Json, Reflection, Reflection.NodeSymbols[start],
                          Settings, false, true);

            start += PrintNodeRecursive(Reflection, start, Json, Settings);
            ++start;
          });

        childrenToSkip = start - NodeId - 1;
      });
  }

  // Children

  uint32_t start = NodeId + 1 + childrenToSkip;
  uint32_t end = NodeId + 1 + node.GetChildCount();

  PrintChildren(Reflection, Json, "Children", start, end, Settings);

  return nodeChildCountForRet;
}

// IsHumanFriendly = false: Raw view of the real file data
// IsHumanFriendly = true:  Clean view that's relatively close to the real tree
std::string ReflectionData::ToJson(bool HideFileInfo,
                                   bool IsHumanFriendly) const {

  JsonWriter json;

  {
    JsonWriter::ObjectScope root(json);

    // Features

    PrintFeatures(Features, json);

    bool hasSymbols = Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

    ReflectionPrintSettings settings{};
    settings.HideFileInfo = HideFileInfo;
    settings.HumanReadable = IsHumanFriendly;

    // Print raw contents

    if (!IsHumanFriendly) {

      json.Array("Strings", [this, &json] {
        for (const std::string &s : Strings)
          json.Value(s);
      });

      json.Array("StringsNonDebug", [this, &json] {
        for (const std::string &s : StringsNonDebug)
          json.Value(s);
      });

      json.Array("Sources", [this, &json] {
        for (uint32_t id : Sources)
          json.Value(Strings[id]);
      });

      json.Array("SourcesAsId", [this, &json] {
        for (uint32_t id : Sources)
          json.Value(uint64_t(id));
      });

      json.Array("Nodes", [this, &json, &settings] {
        for (uint32_t i = 0; i < uint32_t(Nodes.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);
          PrintNode(json, *this, i, settings);
        }
      });

      json.Array("Registers", [this, &json, &settings] {
        for (uint32_t i = 0; i < uint32_t(Registers.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);
          PrintRegister(json, *this, i, settings);
        }
      });

      json.Array("Functions", [this, &json, HideFileInfo, &settings] {
        for (uint32_t i = 0; i < uint32_t(Functions.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);
          PrintFunction(json, *this, i, settings);
        }
      });

      // Already referenced indirectly through other properties we printed
      // before, still printing it to allow consistency checks.
      // For pure pretty prints, you should use the human version.

      json.Array("Parameters", [this, &json, hasSymbols, &settings] {
        for (uint32_t i = 0; i < uint32_t(Parameters.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);

          const ReflectionFunctionParameter &param = Parameters[i];
          std::string paramName =
              hasSymbols ? Strings[NodeSymbols[param.NodeId].GetNameId()]
                         : std::to_string(i);

          json.StringField("ParamName", paramName);

          const ReflectionNode &node = Nodes[param.NodeId];

          PrintParameter(*this, param.TypeId, hasSymbols, json,
                         node.GetSemanticId(), node.GetInterpolationMode(),
                         param.Flags, settings);
        }
      });

      json.Array("Enums", [this, &json, hasSymbols, HideFileInfo, &settings] {
        for (uint32_t i = 0; i < uint32_t(Enums.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);
          PrintEnum(json, *this, i, settings);
        }
      });

      json.Array(
          "EnumValues", [this, &json, hasSymbols, HideFileInfo, &settings] {
            for (uint32_t i = 0; i < uint32_t(EnumValues.size()); ++i) {
              JsonWriter::ObjectScope valueRoot(json);
              PrintEnumValue(json, *this, EnumValues[i].NodeId, settings);
            }
          });

      json.Array("Annotations", [this, &json, hasSymbols] {
        for (uint32_t i = 0; i < uint32_t(Annotations.size()); ++i) {
          const ReflectionAnnotation &annot = Annotations[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.UIntField("StringId", annot.GetStringNonDebug());
          PrintAnnotation(json, *this, annot);
        }
      });

      json.Array("Arrays", [this, &json] {
        for (uint32_t i = 0; i < uint32_t(Arrays.size()); ++i) {
          const ReflectionArray &arr = Arrays[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.UIntField("ArrayElem", arr.ArrayElem());
          json.UIntField("ArrayStart", arr.ArrayStart());
          json.Array("ArraySizes", [this, &json, &arr] {
            for (uint32_t i = 0; i < arr.ArrayElem(); ++i) {
              json.Value(uint64_t(ArraySizes[arr.ArrayStart() + i]));
            }
          });
        }
      });

      json.Array("ArraySizes", [this, &json] {
        for (uint32_t id : ArraySizes)
          json.Value(uint64_t(id));
      });

      json.Array("Members", [this, &json, hasSymbols, &settings] {
        for (uint32_t i = 0; i < uint32_t(MemberTypeIds.size()); ++i) {

          JsonWriter::ObjectScope valueRoot(json);

          if (hasSymbols) {
            json.StringField("Name", Strings[MemberNameIds[i]]);
            json.UIntField("NameId", MemberNameIds[i]);
          }

          PrintTypeName(*this, MemberTypeIds[i], hasSymbols, settings, json,
                        "TypeName");
        }
      });

      json.Array("TypeList", [this, &json, hasSymbols, &settings] {
        for (uint32_t id : TypeList) {
          JsonWriter::ObjectScope valueRoot(json);
          PrintTypeName(*this, id, hasSymbols, settings, json);
        }
      });

      json.Array("Types", [this, &json, hasSymbols, &settings] {
        for (uint32_t i = 0; i < uint32_t(Types.size()); ++i) {
          JsonWriter::ObjectScope nodeRoot(json);
          PrintType(*this, i, hasSymbols, settings, json, false);
        }
      });

      json.Array("Buffers", [this, &json, hasSymbols, &settings] {
        for (uint32_t i = 0; i < uint32_t(Buffers.size()); ++i)
          PrintBuffer(*this, i, hasSymbols, settings, json);
      });

      json.Array("Statements", [this, &json] {
        for (uint32_t i = 0; i < uint32_t(Statements.size()); ++i) {

          const ReflectionScopeStmt &stat = Statements[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.StringField(
              "Type", NodeTypeToString(Nodes[stat.GetNodeId()].GetNodeType()));
          json.UIntField("NodeId", stat.GetNodeId());

          PrintStatement(*this, stat, json);
        }
      });

      json.Array("IfSwitchStatements", [this, &json] {
        for (uint32_t i = 0; i < uint32_t(IfSwitchStatements.size()); ++i) {

          const ReflectionIfSwitchStmt &stat = IfSwitchStatements[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.StringField(
              "Type", NodeTypeToString(Nodes[stat.GetNodeId()].GetNodeType()));
          json.UIntField("NodeId", stat.GetNodeId());

          PrintIfSwitchStatement(*this, stat, json);
        }
      });

      json.Array("BranchStatements", [this, &json] {
        for (uint32_t i = 0; i < uint32_t(BranchStatements.size()); ++i) {

          const ReflectionBranchStmt &stat = BranchStatements[i];
          JsonWriter::ObjectScope valueRoot(json);
          json.StringField(
              "Type", NodeTypeToString(Nodes[stat.GetNodeId()].GetNodeType()));
          json.UIntField("NodeId", stat.GetNodeId());

          PrintBranchStatement(*this, stat, json);
        }
      });
    }

    else
      PrintChildren(*this, json, "Children", 1, Nodes.size(), settings);
  }
  return json.str();
}

} // namespace hlsl
