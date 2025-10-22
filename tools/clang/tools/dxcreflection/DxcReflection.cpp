///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcReflection.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/Attr.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/HlslTypes.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "dxc/DxcReflection/DxcReflection.h"

#undef min
#undef max

using namespace clang;

namespace hlsl {
	
struct DxcRegisterTypeInfo {
  D3D_SHADER_INPUT_TYPE RegisterType;
  D3D_SHADER_INPUT_FLAGS RegisterFlags;
  D3D_SRV_DIMENSION TextureDimension;
  D3D_RESOURCE_RETURN_TYPE TextureValue;
};

static uint32_t RegisterString(DxcHLSLReflectionData &Refl,
    const std::string &Name, bool isNonDebug) {

  if (Name.size() >= 32768)
    throw std::invalid_argument("Strings are limited to 32767");

  if (isNonDebug) {

    if (Refl.StringsNonDebug.size() >= uint32_t(-1))
      throw std::invalid_argument("Strings overflow");

    auto it = Refl.StringsToIdNonDebug.find(Name);

    if (it != Refl.StringsToIdNonDebug.end())
      return it->second;

    uint32_t stringId = uint32_t(Refl.StringsNonDebug.size());

    Refl.StringsNonDebug.push_back(Name);
    Refl.StringsToIdNonDebug[Name] = stringId;
    return stringId;
  }

  if (Refl.Strings.size() >= uint32_t(-1))
    throw std::invalid_argument("Strings overflow");

  auto it = Refl.StringsToId.find(Name);

  if (it != Refl.StringsToId.end())
    return it->second;

  uint32_t stringId = uint32_t(Refl.Strings.size());

  Refl.Strings.push_back(Name);
  Refl.StringsToId[Name] = stringId;
  return stringId;
}

static uint32_t
PushNextNodeId(DxcHLSLReflectionData &Refl, const SourceManager &SM,
               const LangOptions &LangOpts, const std::string &UnqualifiedName,
               const Decl *DeclSelf, D3D12_HLSL_NODE_TYPE Type,
               uint32_t ParentNodeId, uint32_t LocalId,
               const SourceRange *Range = nullptr,
               std::unordered_map<const Decl *, uint32_t> *FwdDecls = nullptr)
{
  
  if (Refl.Nodes.size() >= (1u << 24))
    throw std::invalid_argument("Nodes overflow");

  if (LocalId >= (1u << 24))
    throw std::invalid_argument("LocalId overflow");

  uint32_t nodeId = Refl.Nodes.size();

  uint16_t annotationStart = uint16_t(Refl.Annotations.size());
  uint8_t annotationCount = 0;

  uint16_t semanticId = uint16_t(-1);

  if (DeclSelf) {
    for (const Attr *attr : DeclSelf->attrs()) {
      if (const AnnotateAttr *annotate = dyn_cast<AnnotateAttr>(attr)) {

        if (Refl.Annotations.size() >= (1u << 16))
          throw std::invalid_argument("Out of annotations");

        Refl.Annotations.push_back(DxcHLSLAnnotation(
            RegisterString(Refl, annotate->getAnnotation().str(), true),
            false));

        if (annotationCount >= uint8_t(-1))
          throw std::invalid_argument("Annotation count out of bounds");

        ++annotationCount;

      } else if (const HLSLShaderAttr *shaderAttr =
                     dyn_cast<HLSLShaderAttr>(attr)) {

        if (Refl.Annotations.size() >= (1u << 16))
          throw std::invalid_argument("Out of annotations");

        Refl.Annotations.push_back(DxcHLSLAnnotation(
            RegisterString(
                Refl, "shader(\"" + shaderAttr->getStage().str() + "\")", true),
            true));

        if (annotationCount >= uint8_t(-1))
          throw std::invalid_argument("Annotation count out of bounds");

        ++annotationCount;

      }
    }

    if (const ValueDecl *valDecl = dyn_cast<ValueDecl>(DeclSelf)) {

      const ArrayRef<hlsl::UnusualAnnotation *> &UA =
          valDecl->getUnusualAnnotations();

      for (auto It = UA.begin(), E = UA.end(); It != E; ++It)
        if ((*It)->getKind() == hlsl::UnusualAnnotation::UA_SemanticDecl) {
          semanticId = RegisterString(
              Refl, cast<hlsl::SemanticDecl>(*It)->SemanticName.str(), true);
        }
    }
  }

  bool isFwdDeclare = false;
  bool canHaveFwdDeclare = false;
  const Decl *fwdDeclare = nullptr;

  if (DeclSelf) {
  
    if (const FunctionDecl *func = dyn_cast<FunctionDecl>(DeclSelf)) {
      isFwdDeclare = !func->doesThisDeclarationHaveABody();
      fwdDeclare = func->getCanonicalDecl();
      canHaveFwdDeclare = true;
    }
  
    else if (const EnumDecl *enm = dyn_cast<EnumDecl>(DeclSelf)) {
      isFwdDeclare = !enm->isCompleteDefinition();
      fwdDeclare = enm->getCanonicalDecl();
      canHaveFwdDeclare = true;
    }
  
    else if (const RecordDecl *rec = dyn_cast<RecordDecl>(DeclSelf)) {

      isFwdDeclare = !rec->isThisDeclarationADefinition();
      fwdDeclare = rec->getCanonicalDecl();
      canHaveFwdDeclare = true;

      if (isFwdDeclare && rec->isImplicit())    //Inner ghost node
        return uint32_t(-1);
    }
  }

  uint32_t currId = uint32_t(Refl.Nodes.size());

  if (canHaveFwdDeclare) {

    assert(FwdDecls && "Fwd decl requires FwdDecls map to be present");

    // Multiple fwd declare, ignore
    if (isFwdDeclare && FwdDecls->find(fwdDeclare) != FwdDecls->end())
      return uint32_t(-1);
    
    if (isFwdDeclare)
        (*FwdDecls)[fwdDeclare] = currId;
  }

  Refl.Nodes.push_back(DxcHLSLNode{Type, isFwdDeclare, LocalId, annotationStart,
                                   0, ParentNodeId, annotationCount,
                                   semanticId});

  if (Refl.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) {

    uint16_t sourceLineCount = 0;
    uint32_t sourceLineStart = 0;
    uint32_t sourceColumnStart = 0;
    uint32_t sourceColumnEnd = 0;

    uint16_t sourceId = uint16_t(-1);

    SourceRange range = DeclSelf ? DeclSelf->getSourceRange()
                                 : (Range ? *Range : SourceRange());

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

        if (fileName != presumedEnd.getFilename())
          throw std::invalid_argument("End and start are not in the same file");

        auto it = Refl.StringToSourceId.find(fileName);
        uint32_t i;

        if (it == Refl.StringToSourceId.end()) {
          i = (uint32_t)Refl.Sources.size();
          Refl.Sources.push_back(RegisterString(Refl, fileName, false));
          Refl.StringToSourceId[fileName] = i;
        }

        else {
          i = it->second;
        }

        if (i >= 65535)
          throw std::invalid_argument("Source file count is limited to 16-bit");

        if ((endLine - startLine) >= 65535)
          throw std::invalid_argument("Source line count is limited to 16-bit");

        if (startLine >= 1048576)
          throw std::invalid_argument("Source line start is limited to 20-bit");

        if (startCol >= (1u << 22))
          throw std::invalid_argument("Column start is limited to 22-bit");

        if (endCol >= (1u << 22))
          throw std::invalid_argument("Column end is limited to 22-bit");

        sourceLineCount = uint16_t(endLine - startLine + 1);
        sourceLineStart = startLine;
        sourceColumnStart = startCol;
        sourceColumnEnd = endCol;
        sourceId = uint16_t(i);
      }
    }

    uint32_t nameId = RegisterString(Refl, UnqualifiedName, false);

    Refl.NodeSymbols.push_back(
        DxcHLSLNodeSymbol(nameId, sourceId, sourceLineCount, sourceLineStart,
                          sourceColumnStart, sourceColumnEnd));
  }

  // Link

  if (DeclSelf && fwdDeclare != DeclSelf && fwdDeclare && !isFwdDeclare) {
    assert(FwdDecls && "Referencing fwd decl requires FwdDecls map to be present");
    uint32_t fwd = (*FwdDecls)[fwdDeclare];
    Refl.Nodes[fwd].ResolveFwdDeclare(fwd, Refl.Nodes[currId], currId);
  }

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

  //Parse return type and dimensions

  const ClassTemplateSpecializationDecl *textureTemplate =
      dyn_cast<ClassTemplateSpecializationDecl>(RecordDecl);

  assert(textureTemplate && "Expected texture template");

  const ArrayRef<TemplateArgument>& textureParams = textureTemplate->getTemplateArgs().asArray();

  bool shouldBeTexture2DMS = textureParams.size() == 2;

  if (shouldBeTexture2DMS)
    assert(textureParams[0].getKind() == TemplateArgument::Type &&
           textureParams[1].getKind() == TemplateArgument::Integral &&
           "Expected template args");

  else assert(textureParams.size() == 1 &&
         textureParams[0].getKind() == TemplateArgument::Type &&
         "Expected template args");

  QualType valueType = textureParams[0].getAsType();
  QualType desugared = valueType.getDesugaredType(ASTCtx);

  uint32_t dimensions;

  if (const BuiltinType *bt = dyn_cast<BuiltinType>(desugared))
    dimensions = 1;

  else {

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

    assert(vectorParams.size() == 2 &&
           vectorParams[0].getKind() == TemplateArgument::Type &&
           vectorParams[1].getKind() == TemplateArgument::Integral &&
           "Expected vector to be vector<T, N>");

    valueType = vectorParams[0].getAsType();
    desugared = valueType.getDesugaredType(ASTCtx);

    dimensions = vectorParams[1].getAsIntegral().getZExtValue();
  }

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

  switch (dimensions) {
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
    assert(!shouldBeTexture2DMS && "Buffer<T> expected but got Buffer<T, ...>");
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

  else if (TypeName == "Texture2DMS")
    type.TextureDimension = D3D_SRV_DIMENSION_TEXTURE2DMS;

  assert((shouldBeTexture2DMS ==
          (type.TextureDimension == D3D_SRV_DIMENSION_TEXTURE2DMS)) &&
         "Texture2DMS used with Texture2D syntax or reverse");

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

  if (typeName.size() >= 17 && typeName.substr(0, 17) == "RasterizerOrdered")
    typeName = typeName.substr(17);

  if (typeName == "SamplerState" || typeName == "SamplerComparisonState") {
    return {D3D_SIT_SAMPLER, typeName == "SamplerComparisonState"
                                 ? D3D_SIF_COMPARISON_SAMPLER
                                 : (D3D_SHADER_INPUT_FLAGS)0};
  }
  
  DxcRegisterTypeInfo info = {};

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

static uint32_t PushArray(DxcHLSLReflectionData &Refl, uint32_t ArraySizeFlat,
                          const std::vector<uint32_t> &ArraySize) {

  if (ArraySizeFlat <= 1 || ArraySize.size() <= 1)
    return uint32_t(-1);

  if (Refl.Arrays.size() >= uint32_t((1u << 31) - 1))
    throw std::invalid_argument("Arrays would overflow");

  uint32_t arrayId = uint32_t(Refl.Arrays.size());

  uint32_t arrayCountStart = uint32_t(Refl.ArraySizes.size());
  uint32_t numArrayElements = std::min(size_t(32), ArraySize.size());

  if (Refl.ArraySizes.size() + numArrayElements >= ((1u << 26) - 1))
    throw std::invalid_argument("Array elements would overflow");

  for (uint32_t i = 0; i < ArraySize.size() && i < 8; ++i) {

    uint32_t arraySize = ArraySize[i];

    // Flatten rest of array to at least keep consistent array elements
    if (i == 31)
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

static void RegisterTypeList(DxcHLSLReflectionData &Refl,
                      const std::vector<uint32_t> &TypeIds, uint32_t &Offset,
                      uint8_t &Len) {

  if (TypeIds.empty())
    return;

  if (TypeIds.size() >= uint8_t(-1))
    throw std::invalid_argument("Only allowing 256 types in a type list");

  uint32_t i = 0;
  uint32_t j = uint32_t(Refl.TypeList.size());
  uint32_t k = 0;

  Offset = 0;

  for (; i < j; ++i) {

    if (k == TypeIds.size())
      break;

    if (TypeIds[k] != Refl.TypeList[i]) {

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

    uint32_t oldSiz = uint32_t(Refl.TypeList.size());

    if (oldSiz + TypeIds.size() >= (1u << 24))
      throw std::invalid_argument("Only allowing 16Mi total interfaces");

    Refl.TypeList.resize(oldSiz + TypeIds.size());

    std::memcpy(Refl.TypeList.data() + oldSiz, TypeIds.data(),
                TypeIds.size() * sizeof(uint32_t));

    Offset = oldSiz;
  }

  Len = uint8_t(TypeIds.size());
}

uint32_t GenerateTypeInfo(ASTContext &ASTCtx, DxcHLSLReflectionData &Refl,
                          QualType Original, bool DefaultRowMaj) {

  // Unwrap array
  // There's the following issue:
  // Let's say the underlying type is F32x4[4] but the sugared name is F32x4x4,
  //  then we want to maintain sugared name + array info (of sugar) for reflection 
  //  but for low level type info, we would want to know float4[4]

  uint32_t arraySizeUnderlying = 1;
  QualType underlying = Original.getNonReferenceType().getCanonicalType();
  std::vector<uint32_t> arrayElemUnderlying;

  while (const ConstantArrayType *arr =
             dyn_cast<ConstantArrayType>(underlying)) {
    uint32_t current = arr->getSize().getZExtValue();
    arrayElemUnderlying.push_back(current);
    arraySizeUnderlying *= arr->getSize().getZExtValue();
    underlying = arr->getElementType().getNonReferenceType().getCanonicalType();
  }

  uint32_t arraySizeDisplay = 1;
  std::vector<uint32_t> arrayElemDisplay;
  QualType display = Original.getNonReferenceType();

  while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(display)) {
    uint32_t current = arr->getSize().getZExtValue();
    arrayElemDisplay.push_back(current);
    arraySizeDisplay *= arr->getSize().getZExtValue();
    display = arr->getElementType().getNonReferenceType();
  }

  // Name; Omit struct, class and const keywords

  PrintingPolicy policy(ASTCtx.getLangOpts());
  policy.SuppressScope = false;
  policy.AnonymousTagLocations = false;
  policy.SuppressTagKeyword = true;

  display = display.getUnqualifiedType();
  std::string displayName = display.getAsString(policy);

  std::string underlyingName =
      underlying.getUnqualifiedType().getAsString(policy);

  //Prune template instantiation from type name for builtin types (ex. vector & matrix)
  //But only if it's not a sugared type:
  // typedef ConstantBuffer<Test> MyTest;
  //In this case, MyTest will still be seen as a ConstantBuffer<Test> but the typeName is MyTest.

  static const std::unordered_map<std::string, D3D_SHADER_VARIABLE_TYPE>
      lookup = std::unordered_map<std::string, D3D_SHADER_VARIABLE_TYPE>{
          {{"vector", D3D_SHADER_VARIABLE_TYPE(-1)},
           {"matrix", D3D_SHADER_VARIABLE_TYPE(-2)},
           {"Texture1D", D3D_SVT_TEXTURE1D},
           {"Texture2D", D3D_SVT_TEXTURE2D},
           {"RWTexture1D", D3D_SVT_RWTEXTURE1D},
           {"RWTexture2D", D3D_SVT_RWTEXTURE2D},
           {"Texture2DMS", D3D_SVT_TEXTURE2DMS},
           {"Texture3D", D3D_SVT_TEXTURE3D},
           {"RWTexture3D", D3D_SVT_RWTEXTURE3D},
           {"TextureCube", D3D_SVT_TEXTURECUBE},
           {"Texture1DArray", D3D_SVT_TEXTURE1DARRAY},
           {"Texture2DArray", D3D_SVT_TEXTURE2DARRAY},
           {"RWTexture1DArray", D3D_SVT_RWTEXTURE1DARRAY},
           {"RWTexture2DArray", D3D_SVT_RWTEXTURE2DARRAY},
           {"Texture2DMSArray", D3D_SVT_TEXTURE2DMSARRAY},
           {"TextureCubeArray", D3D_SVT_TEXTURECUBEARRAY},
           {"SamplerState", D3D_SVT_SAMPLER},
           {"ByteAddressBuffer", D3D_SVT_BYTEADDRESS_BUFFER},
           {"RWByteAddressBuffer", D3D_SVT_RWBYTEADDRESS_BUFFER},
           {"StructuredBuffer", D3D_SVT_STRUCTURED_BUFFER},
           {"RWStructuredBuffer", D3D_SVT_RWSTRUCTURED_BUFFER},
           {"AppendStructuredBuffer", D3D_SVT_APPEND_STRUCTURED_BUFFER},
           {"ConsumeStructuredBuffer", D3D_SVT_CONSUME_STRUCTURED_BUFFER},
           {"RWBuffer", D3D_SVT_RWBUFFER},
           {"Buffer", D3D_SVT_BUFFER},
           {"ConstantBuffer", D3D_SVT_CBUFFER}}};

  if (const TemplateSpecializationType *spec =
          dyn_cast<TemplateSpecializationType>(display.getTypePtr())) {

    const TemplateDecl *td = spec->getTemplateName().getAsTemplateDecl();

    if (td) {

      auto it = lookup.find(td->getName());

      if (it != lookup.end()) {

        if(it->second >= 0)
          displayName = underlyingName = it->first;
      }
    }
  }

  bool hasSymbols = Refl.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  //Two arrays; for display and for underlying

  uint32_t arrayIdUnderlying = PushArray(Refl, arraySizeUnderlying, arrayElemUnderlying);
  DxcHLSLArrayOrElements elementsOrArrayIdUnderlying(arrayIdUnderlying, arraySizeUnderlying);

  uint32_t arrayIdDisplay = PushArray(Refl, arraySizeDisplay, arrayElemDisplay);
  DxcHLSLArrayOrElements elementsOrArrayIdDisplay(arrayIdDisplay, arraySizeDisplay);

  //Unwrap vector and matrix
  //And base type

  D3D_SHADER_VARIABLE_CLASS cls = D3D_SVC_STRUCT;

  if (const RecordType *RT = underlying->getAs<RecordType>()) {

    const RecordDecl *RD = RT->getDecl();

    if (RD->getTagKind() == TTK_Interface)
      cls = D3D_SVC_INTERFACE_CLASS;
  }

  uint8_t rows = 0, columns = 0;

  uint32_t membersCount = 0;
  uint32_t membersOffset = 0;

  uint32_t baseType = uint32_t(-1);
  std::vector<uint32_t> interfaces;

  D3D_SHADER_VARIABLE_TYPE type = D3D_SVT_VOID;

  if (const RecordType *record = underlying->getAs<RecordType>()) {

     bool standardType = false;

     RecordDecl *recordDecl = record->getDecl();

     QualType innerType;
     std::string innerTypeName;     //$Element or T depending on type

    if (const ClassTemplateSpecializationDecl *templateClass =
            dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {

      std::string name = templateClass->getIdentifier()->getName();

      const ArrayRef<TemplateArgument> &params =
          templateClass->getTemplateArgs().asArray();

      auto it = lookup.find(name);

      if (it != lookup.end()) {
      
        D3D_SHADER_VARIABLE_TYPE svt = it->second;

        if (svt == -1) {        //Reserved as 'vector'

          rows = 1;

          assert(params.size() == 2 &&
                 params[0].getKind() == TemplateArgument::Type &&
                 params[1].getKind() == TemplateArgument::Integral &&
                 "Expected vector to be vector<T, N>");

          underlying = params[0].getAsType();
          columns = params[1].getAsIntegral().getSExtValue();
          cls = D3D_SVC_VECTOR;
          standardType = true;
        }

        else if (svt == -2) {   //Reserved as 'matrix'

          assert(params.size() == 3 &&
                 params[0].getKind() == TemplateArgument::Type &&
                 params[1].getKind() == TemplateArgument::Integral &&
                 params[2].getKind() == TemplateArgument::Integral &&
                 "Expected matrix to be matrix<T, C, R>");

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

        else {

          type = svt;
          cls = D3D_SVC_OBJECT;

          innerTypeName = "$Element";

          const TemplateSpecializationType *templateDesc =
              display->getAs<TemplateSpecializationType>();

          assert(templateDesc && "Expected a valid TemplateSpecializationType");
          innerType = templateDesc->getArg(0).getAsType();

          if (svt == D3D_SVT_RWBUFFER || svt == D3D_SVT_BUFFER || svt == D3D_SVT_CBUFFER)
            innerTypeName = innerType.getUnqualifiedType().getAsString(policy);
        }
      }
    }

    else {

      std::string name = recordDecl->getName();

      auto it = lookup.find(name);

      if (it != lookup.end()) {
        type = it->second;
        cls = D3D_SVC_OBJECT;
      }
    }

    // Buffer types have a member to allow inspection of the types

    if (innerTypeName.size()) {

      uint32_t nameId = hasSymbols ? RegisterString(Refl, innerTypeName, false)
                                   : uint32_t(-1);

      uint32_t typeId =
          GenerateTypeInfo(ASTCtx, Refl, innerType, DefaultRowMaj);

      membersOffset = uint32_t(Refl.MemberTypeIds.size());
      membersCount = 1;

      Refl.MemberTypeIds.push_back(typeId);

      if (hasSymbols)
        Refl.MemberNameIds.push_back(nameId);
    }

    // Fill members

    if (!standardType && recordDecl->isCompleteDefinition() && cls != D3D_SVC_OBJECT) {

      // Base types

      if (CXXRecordDecl *cxxRecordDecl = dyn_cast<CXXRecordDecl>(recordDecl))
        if (cxxRecordDecl->getNumBases()) {
          for (auto &I : cxxRecordDecl->bases()) {

            QualType qualType = I.getType();
            CXXRecordDecl *BaseDecl =
                cast<CXXRecordDecl>(qualType->castAs<RecordType>()->getDecl());

            if (BaseDecl->isInterface()) {
              interfaces.push_back(
                  GenerateTypeInfo(ASTCtx, Refl, qualType, DefaultRowMaj));
              continue;
            }

            assert(baseType == uint32_t(-1) &&
                   "Multiple base types isn't supported in HLSL");

            baseType = GenerateTypeInfo(ASTCtx, Refl, qualType, DefaultRowMaj);
          }
        }

      // Inner types

      // Reserve member names and types

      for (Decl *decl : recordDecl->decls()) {

        FieldDecl *fieldDecl = dyn_cast<FieldDecl>(decl);

        if (!fieldDecl)
          continue;

        if (!membersCount)
          membersOffset = uint32_t(Refl.MemberTypeIds.size());

        std::string name = fieldDecl->getName();

        uint32_t nameId =
            hasSymbols ? RegisterString(Refl, name, false) : uint32_t(-1);

        if (hasSymbols)
          Refl.MemberNameIds.push_back(nameId);

        ++membersCount;
      }

      if (membersCount) {

        Refl.MemberTypeIds.resize(Refl.MemberTypeIds.size() + membersCount);

        if (Refl.MemberTypeIds.size() >= uint32_t(1u << 24))
          throw std::invalid_argument("Members out of bounds");
      }

      // Initialize member types (because it causes recursion)

      membersCount = 0;

      for (Decl *decl : recordDecl->decls()) {

        FieldDecl *fieldDecl = dyn_cast<FieldDecl>(decl);

        if (!fieldDecl)
          continue;

        Refl.MemberTypeIds[membersOffset + membersCount] =
            GenerateTypeInfo(ASTCtx, Refl, fieldDecl->getType(), DefaultRowMaj);

        ++membersCount;
      }
    }
  }

  //Type name

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
      underlyingName = "min10float";
      break;

    case BuiltinType::Min16Float:
      type = D3D_SVT_MIN16FLOAT;
      underlyingName = "min16float";
      break;

    case BuiltinType::HalfFloat:
    case BuiltinType::Half:
      type = D3D_SVT_FLOAT16;
      underlyingName = "float16_t";      //TODO: half or float16_t?
      break;

    case BuiltinType::Short:
      type = D3D_SVT_INT16;
      underlyingName = "int16_t";
      break;

    case BuiltinType::Min12Int:
      type = D3D_SVT_MIN12INT;
      underlyingName = "min12int";
      break;

    case BuiltinType::Min16Int:
      type = D3D_SVT_MIN16INT;
      underlyingName = "min16int";
      break;

    case BuiltinType::Min16UInt:
      type = D3D_SVT_MIN16UINT;
      underlyingName = "min16uint";
      break;

    case BuiltinType::UShort:
      type = D3D_SVT_UINT16;
      underlyingName = "uint16_t";
      break;

    case BuiltinType::Float:
      type = D3D_SVT_FLOAT;
      underlyingName = "float";
      break;

    case BuiltinType::Int:
      type = D3D_SVT_INT;
      underlyingName = "int";
      break;

    case BuiltinType::UInt:
      type = D3D_SVT_UINT;
      underlyingName = "uint";
      break;

    case BuiltinType::Bool:
      type = D3D_SVT_BOOL;
      underlyingName = "bool";
      break;

    case BuiltinType::Double:
      type = D3D_SVT_DOUBLE;
      underlyingName = "double";
      break;

    case BuiltinType::ULongLong:
      type = D3D_SVT_UINT64;
      underlyingName = "uint64_t";
      break;

    case BuiltinType::LongLong:
      type = D3D_SVT_INT64;
      underlyingName = "int64_t";
      break;

    default:
      throw std::invalid_argument("Invalid builtin type");
    }
  }

  //Turn into proper fully qualified name (e.g. turn vector<float, 4> into float4)

  switch (cls) {

  case D3D_SVC_MATRIX_ROWS:
  case D3D_SVC_VECTOR:

    underlyingName += std::to_string(columns);

    if (cls == D3D_SVC_MATRIX_ROWS)
      underlyingName += "x" + std::to_string(rows);

    break;

  case D3D_SVC_MATRIX_COLUMNS:
    underlyingName += std::to_string(rows) + "x" + std::to_string(columns);
    break;
  }

  //Insert

  if (Refl.Types.size() >= uint32_t(-1))
    throw std::invalid_argument("Type id out of bounds");

  if (interfaces.size() >= uint8_t(-1))
    throw std::invalid_argument("Only allowing 256 interfaces");

  uint32_t interfaceOffset = 0;
  uint8_t interfaceCount = 0;
  RegisterTypeList(Refl, interfaces, interfaceOffset, interfaceCount);

  DxcHLSLType hlslType(baseType, elementsOrArrayIdUnderlying, cls, type, rows, columns,
                       membersCount, membersOffset, interfaceOffset,
                       interfaceCount);

  uint32_t displayNameId =
      hasSymbols ? RegisterString(Refl, displayName, false) : uint32_t(-1);

  uint32_t underlyingNameId =
      hasSymbols ? RegisterString(Refl, underlyingName, false) : uint32_t(-1);

  DxcHLSLTypeSymbol typeSymbol(elementsOrArrayIdDisplay, displayNameId,
                               underlyingNameId);

  uint32_t i = 0;
  uint32_t j = uint32_t(Refl.Types.size());

  for (; i < j; ++i)
    if (Refl.Types[i] == hlslType &&
        (!hasSymbols || Refl.TypeSymbols[i] == typeSymbol))
      break;

  if (i == j) {

    if (hasSymbols)
      Refl.TypeSymbols.push_back(typeSymbol);

    Refl.Types.push_back(hlslType);
  }

  return i;
}

static D3D_CBUFFER_TYPE GetBufferType(uint8_t Type) {

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
    DxcHLSLReflectionData &Refl, uint32_t AutoBindingSpace, uint32_t ParentNodeId,
    bool DefaultRowMaj) {

  ArrayRef<hlsl::UnusualAnnotation *> UA = ValDesc->getUnusualAnnotations();

  DxcRegisterTypeInfo inputType = GetRegisterTypeInfo(ASTCtx, Type);

  uint32_t nodeId =
      PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), ValDesc->getName(),
                     ValDesc, D3D12_HLSL_NODE_TYPE_REGISTER, ParentNodeId,
                     uint32_t(Refl.Registers.size()));

  uint32_t arrayId = PushArray(Refl, ArraySizeFlat, ArraySize);

  uint32_t bufferId = 0;
  D3D_CBUFFER_TYPE bufferType = GetBufferType(inputType.RegisterType);
  
  if(bufferType != D3D_CT_INTERFACE_POINTERS) {
    bufferId = uint32_t(Refl.Buffers.size());
    Refl.Buffers.push_back({bufferType, nodeId});
  }

  DxcHLSLRegister regD3D12 = {
      inputType.RegisterType,
      ArraySizeFlat,
      uint32_t(inputType.RegisterFlags),
      inputType.TextureValue,
      inputType.TextureDimension,
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

    const TemplateSpecializationType *templateDesc =
        Type->getAs<TemplateSpecializationType>();

    assert(templateDesc->getNumArgs() == 1 &&
           templateDesc->getArg(0).getKind() == TemplateArgument::Type &&
           "Expected Type<T>");

    QualType innerType = templateDesc->getArg(0).getAsType();

    // The name of the inner struct is $Element if 'array', otherwise equal to
    // register name

    uint32_t typeId = GenerateTypeInfo(ASTCtx, Refl, innerType, DefaultRowMaj);

    SourceRange sourceRange = ValDesc->getSourceRange();

    PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(),
                   isListType ? "$Element" : ValDesc->getName(), nullptr,
                   D3D12_HLSL_NODE_TYPE_VARIABLE, nodeId, typeId, &sourceRange);

    break;
  }
  }
}

//TODO: Debug code
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
                   DxcHLSLReflectionData &Refl, const T &Decls,
                   bool DefaultRowMaj, uint32_t ParentId) {

  for (Decl *decl : Decls) {

    ValueDecl *valDecl = dyn_cast<ValueDecl>(decl);
    assert(valDecl && "Decl was expected to be a ValueDecl but wasn't");
    QualType original = valDecl->getType();

    const std::string &name = valDecl->getName();

    uint32_t typeId = GenerateTypeInfo(ASTCtx, Refl, original, DefaultRowMaj);

    uint32_t nodeId = PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, decl,
                       D3D12_HLSL_NODE_TYPE_VARIABLE, ParentId, typeId);

    //Handle struct recursion

    if (RecordDecl *recordDecl = dyn_cast<RecordDecl>(decl)) {

      if (!recordDecl->isCompleteDefinition())
        continue;

      RecurseBuffer(ASTCtx, SM, Refl, recordDecl->fields(), DefaultRowMaj, nodeId);
    }
  }
}

uint32_t RegisterBuffer(ASTContext &ASTCtx, DxcHLSLReflectionData &Refl,
                        const SourceManager &SM, DeclContext *Buffer,
                        uint32_t NodeId, D3D_CBUFFER_TYPE Type,
                        bool DefaultRowMaj) {

  if (Refl.Buffers.size() >= uint32_t(-1))
    throw std::invalid_argument("Buffer id out of bounds");

  uint32_t bufferId = uint32_t(Refl.Buffers.size());

  RecurseBuffer(ASTCtx, SM, Refl, Buffer->decls(), DefaultRowMaj, NodeId);

  Refl.Buffers.push_back({Type, NodeId});

  return bufferId;
}

//DxilInterpolationMode.cpp but a little bit cleaned up
static D3D_INTERPOLATION_MODE GetInterpolationMode(Decl *decl) {

  if (!decl)        //Return type
    return D3D_INTERPOLATION_UNDEFINED;

  bool bNoInterpolation = decl->hasAttr<HLSLNoInterpolationAttr>();
  bool bLinear = decl->hasAttr<HLSLLinearAttr>();
  bool bNoperspective = decl->hasAttr<HLSLNoPerspectiveAttr>();
  bool bCentroid = decl->hasAttr<HLSLCentroidAttr>();
  bool bSample = decl->hasAttr<HLSLSampleAttr>();

  uint8_t mask = uint8_t(bNoInterpolation) << 4;
  mask |= uint8_t(bLinear) << 3;
  mask |= uint8_t(bNoperspective) << 2;
  mask |= uint8_t(bCentroid) << 1;
  mask |= uint8_t(bSample);

  if (mask > 16)
    return D3D_INTERPOLATION_UNDEFINED;
  
  static constexpr const D3D_INTERPOLATION_MODE modes[] = {
      D3D_INTERPOLATION_UNDEFINED,
      D3D_INTERPOLATION_LINEAR_SAMPLE,
      D3D_INTERPOLATION_LINEAR_CENTROID,
      D3D_INTERPOLATION_LINEAR_SAMPLE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,
      D3D_INTERPOLATION_LINEAR,
      D3D_INTERPOLATION_LINEAR_SAMPLE,
      D3D_INTERPOLATION_LINEAR_CENTROID,
      D3D_INTERPOLATION_LINEAR_SAMPLE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID,
      D3D_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE,
      D3D_INTERPOLATION_CONSTANT
  };

  return modes[mask];
}

static void AddFunctionParameter(ASTContext &ASTCtx, QualType Type, Decl *Decl,
                                 DxcHLSLReflectionData &Refl,
                                 const SourceManager &SM, uint32_t ParentNodeId,
                                 bool DefaultRowMaj) {

  PrintingPolicy printingPolicy(ASTCtx.getLangOpts());

  uint32_t typeId = GenerateTypeInfo(ASTCtx, Refl, Type, DefaultRowMaj);

  uint32_t nodeId = PushNextNodeId(
      Refl, SM, ASTCtx.getLangOpts(),
      Decl && dyn_cast<NamedDecl>(Decl) ? dyn_cast<NamedDecl>(Decl)->getName()
                                        : "",
      Decl, D3D12_HLSL_NODE_TYPE_PARAMETER, ParentNodeId,
      uint32_t(Refl.Parameters.size()));

  D3D_INTERPOLATION_MODE interpolationMode = GetInterpolationMode(Decl);
  D3D_PARAMETER_FLAGS flags = D3D_PF_NONE;

  if (Decl) {

    if (Decl->hasAttr<HLSLInAttr>())
      flags = D3D_PARAMETER_FLAGS(flags | D3D_PF_IN);

    if (Decl->hasAttr<HLSLOutAttr>())
      flags = D3D_PARAMETER_FLAGS(flags | D3D_PF_OUT);

    if (Decl->hasAttr<HLSLInOutAttr>())
      flags = D3D_PARAMETER_FLAGS(flags | D3D_PF_IN | D3D_PF_OUT);
  }

  Refl.Parameters.push_back(
      DxcHLSLParameter{typeId, nodeId, uint8_t(interpolationMode), uint8_t(flags)});
}

static void RecursiveReflectBody(
    const Stmt *Statement, ASTContext &ASTCtx, DiagnosticsEngine &Diags,
    const SourceManager &SM, DxcHLSLReflectionData &Refl,
    uint32_t AutoBindingSpace, uint32_t Depth,
    D3D12_HLSL_REFLECTION_FEATURE Features, uint32_t ParentNodeId,
    bool DefaultRowMaj, std::unordered_map<const Decl *, uint32_t> &FwdDecls,
    const LangOptions &LangOpts, bool SkipNextCompound = false);

static void GenerateStatement(
    ASTContext &ASTCtx, DiagnosticsEngine &Diags, const SourceManager &SM,
    DxcHLSLReflectionData &Refl, uint32_t AutoBindingSpace, uint32_t Depth,
    D3D12_HLSL_REFLECTION_FEATURE Features, uint32_t ParentNodeId,
    bool DefaultRowMaj, std::unordered_map<const Decl *, uint32_t> &FwdDecls,
    const LangOptions &LangOpts, D3D12_HLSL_NODE_TYPE Type,
    const VarDecl *VarDecl, const Stmt *Body, const Stmt *Init,
    const Stmt *Self, bool IfAndHasElse = false) {

  uint32_t loc = uint32_t(Refl.Statements.size());

  const SourceRange &sourceRange = Self->getSourceRange();

  uint32_t nodeId = PushNextNodeId(Refl, SM, LangOpts, "", nullptr, Type,
                                   ParentNodeId, loc, &sourceRange, &FwdDecls);

  Refl.Statements.push_back(DxcHLSLStatement());

  if (VarDecl) {

    uint32_t typeId =
        GenerateTypeInfo(ASTCtx, Refl, VarDecl->getType(), DefaultRowMaj);

    const SourceRange &sourceRange = VarDecl->getSourceRange();

    PushNextNodeId(Refl, SM, LangOpts, VarDecl->getName(), VarDecl,
                   D3D12_HLSL_NODE_TYPE_VARIABLE, nodeId, typeId, &sourceRange,
                   &FwdDecls);
  }

  uint32_t start = uint32_t(Refl.Nodes.size());

  RecursiveReflectBody(Init, ASTCtx, Diags, SM, Refl, AutoBindingSpace,
                       Depth + 1, Features, nodeId, DefaultRowMaj, FwdDecls,
                       LangOpts, true);

  Refl.Statements[loc] =
      DxcHLSLStatement(nodeId, uint32_t(Refl.Nodes.size() - start), VarDecl, IfAndHasElse);

  RecursiveReflectBody(Body, ASTCtx, Diags, SM, Refl, AutoBindingSpace,
                       Depth + 1, Features, nodeId, DefaultRowMaj, FwdDecls,
                       LangOpts, true);
}

static void RecursiveReflectBody(
    const Stmt *Statement, ASTContext &ASTCtx, DiagnosticsEngine &Diags,
    const SourceManager &SM, DxcHLSLReflectionData &Refl,
    uint32_t AutoBindingSpace, uint32_t Depth,
    D3D12_HLSL_REFLECTION_FEATURE Features, uint32_t ParentNodeId,
    bool DefaultRowMaj, std::unordered_map<const Decl *, uint32_t> &FwdDecls,
    const LangOptions &LangOpts,
    bool SkipNextCompound) {

  if (!Statement)
    return;

  if (const IfStmt *If = dyn_cast<IfStmt>(Statement))
    GenerateStatement(ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
                      Features, ParentNodeId, DefaultRowMaj, FwdDecls, LangOpts,
                      D3D12_HLSL_NODE_TYPE_IF, If->getConditionVariable(),
                      If->getElse(), If->getThen(), If, If->getElse());

  else if (const ForStmt *For = dyn_cast<ForStmt>(Statement))
    GenerateStatement(ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
                           Features, ParentNodeId, DefaultRowMaj, FwdDecls,
                           LangOpts, D3D12_HLSL_NODE_TYPE_FOR,
                           For->getConditionVariable(), For->getBody(),
                           For->getInit(), For);

  else if (const WhileStmt *While = dyn_cast<WhileStmt>(Statement))
    GenerateStatement(ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
                           Features, ParentNodeId, DefaultRowMaj, FwdDecls,
                           LangOpts, D3D12_HLSL_NODE_TYPE_WHILE,
                           While->getConditionVariable(), While->getBody(),
                           nullptr, While);

  else if (const SwitchStmt *Switch = dyn_cast<SwitchStmt>(Statement))
    GenerateStatement(ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
                           Features, ParentNodeId, DefaultRowMaj, FwdDecls,
                           LangOpts, D3D12_HLSL_NODE_TYPE_SWITCH,
                           Switch->getConditionVariable(), Switch->getBody(), nullptr,
                           Switch);

  else if (const DoStmt *Do = dyn_cast<DoStmt>(Statement)) {

    const SourceRange &sourceRange = Do->getSourceRange();

    uint32_t scopeNode =
        PushNextNodeId(Refl, SM, LangOpts, "", nullptr, D3D12_HLSL_NODE_TYPE_DO,
                       ParentNodeId, 0, &sourceRange, &FwdDecls);

    RecursiveReflectBody(Do->getBody(), ASTCtx, Diags, SM, Refl,
                         AutoBindingSpace, Depth + 1, Features, scopeNode,
                         DefaultRowMaj, FwdDecls, LangOpts, true);
  }

  else if (const CompoundStmt *scope = dyn_cast<CompoundStmt>(Statement)) {

    const SourceRange &sourceRange = scope->getSourceRange();

    uint32_t scopeNode = SkipNextCompound ? ParentNodeId : PushNextNodeId(
        Refl, SM, LangOpts, "", nullptr, D3D12_HLSL_NODE_TYPE_SCOPE,
        ParentNodeId, 0, &sourceRange, &FwdDecls);

    for (const Stmt *child : scope->body())
      RecursiveReflectBody(child, ASTCtx, Diags, SM, Refl, AutoBindingSpace,
                           Depth + 1, Features, scopeNode, DefaultRowMaj,
                           FwdDecls, LangOpts);
  }

  else if (const DeclStmt *DS = dyn_cast<DeclStmt>(Statement)) {
    for (Decl *D : DS->decls()) {
      if (VarDecl *varDecl = dyn_cast<VarDecl>(D)) {

        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, varDecl->getType(), DefaultRowMaj);

        const SourceRange &sourceRange = varDecl->getSourceRange();

        PushNextNodeId(Refl, SM, LangOpts, varDecl->getName(), varDecl,
                       D3D12_HLSL_NODE_TYPE_VARIABLE, ParentNodeId, typeId,
                       &sourceRange, &FwdDecls);
      }
    }
  }
}

static void
RecursiveReflectHLSL(const DeclContext &Ctx, ASTContext &ASTCtx,
                     DiagnosticsEngine &Diags, const SourceManager &SM,
                     DxcHLSLReflectionData &Refl, uint32_t AutoBindingSpace,
                     uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
                     uint32_t ParentNodeId, bool DefaultRowMaj,
                     std::unordered_map<const Decl *, uint32_t> &FwdDecls) {

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

  //TODO: scopes (if/switch/for/empty scope)

  for (Decl *it : Ctx.decls()) {

    SourceLocation Loc = it->getLocation();
    if (Loc.isInvalid() || SM.isInSystemHeader(Loc))    //TODO: We might want to include these for a more complete picture.
      continue;

    if (isa<ParmVarDecl>(it))    //Skip parameters, already handled explicitly
      continue;

    if (HLSLBufferDecl *CBuffer = dyn_cast<HLSLBufferDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
        continue;

      // TODO: Add Depth > 0 for reflection if
      // D3D12_HLSL_REFLECTION_FEATURE_SCOPES

      if (Depth != 0)
        continue;

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), CBuffer->getName(),
                         CBuffer, D3D12_HLSL_NODE_TYPE_REGISTER, ParentNodeId,
                         uint32_t(Refl.Registers.size()));

      uint32_t bufferId = RegisterBuffer(ASTCtx, Refl, SM, CBuffer, nodeId,
                                         D3D_CT_CBUFFER, DefaultRowMaj);

      DxcHLSLRegister regD3D12 = {D3D_SIT_CBUFFER,
                                  1,
                                  uint32_t(D3D_SIF_USERPACKED),
                                  D3D_RESOURCE_RETURN_TYPE(0),
                                  D3D_SRV_DIMENSION_UNKNOWN,
                                  nodeId,
                                  uint32_t(-1),
                                  bufferId};

      Refl.Registers.push_back(regD3D12);
    }

    else if (FunctionDecl *Func = dyn_cast<FunctionDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS))
        continue;

      if (Func->isImplicit())
        continue;

      const FunctionDecl *Definition = nullptr;

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), Func->getName(), Func,
                         D3D12_HLSL_NODE_TYPE_FUNCTION, ParentNodeId,
                         uint32_t(Refl.Functions.size()), nullptr, &FwdDecls);

      if (nodeId == uint32_t(-1)) // Duplicate fwd definition
        continue;

      bool hasDefinition = Func->hasBody(Definition);
      DxcHLSLFunction func = {nodeId, Func->getNumParams(),
                              !Func->getReturnType().getTypePtr()->isVoidType(),
                              hasDefinition};

      for (uint32_t i = 0; i < func.GetNumParameters(); ++i)
        AddFunctionParameter(ASTCtx, Func->getParamDecl(i)->getType(),
                             Func->getParamDecl(i), Refl, SM, nodeId,
                             DefaultRowMaj);

      if (func.HasReturn())
        AddFunctionParameter(ASTCtx, Func->getReturnType(), nullptr, Refl, SM,
                             nodeId, DefaultRowMaj);

      Refl.Functions.push_back(std::move(func));

      if (hasDefinition && (Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)) {

        Stmt *stmt = Func->getBody();

        for (const Stmt *subStmt : stmt->children()) {

          if (!subStmt)
            continue;

          RecursiveReflectBody(subStmt, ASTCtx, Diags, SM, Refl,
                               AutoBindingSpace, Depth, Features, nodeId,
                               DefaultRowMaj, FwdDecls, ASTCtx.getLangOpts());
        }
      }
    }

    else if (TypedefDecl *Typedef = dyn_cast<TypedefDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      uint32_t typeId = GenerateTypeInfo(
          ASTCtx, Refl, Typedef->getUnderlyingType(), DefaultRowMaj);

      PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), Typedef->getName(),
                     Typedef, D3D12_HLSL_NODE_TYPE_TYPEDEF, ParentNodeId,
                     typeId);
    }

    else if (TypeAliasDecl *TypeAlias = dyn_cast<TypeAliasDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      // TODO: TypeAlias->print(pfStream, printingPolicy);
    }

    else if (EnumDecl *Enum = dyn_cast<EnumDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      uint32_t nodeId = PushNextNodeId(
          Refl, SM, ASTCtx.getLangOpts(), Enum->getName(), Enum,
                         D3D12_HLSL_NODE_TYPE_ENUM, ParentNodeId,
                         uint32_t(Refl.Enums.size()), nullptr, &FwdDecls);

      if (nodeId == uint32_t(-1)) // Duplicate fwd definition
        continue;

      for (EnumConstantDecl *EnumValue : Enum->enumerators()) {

        uint32_t childNodeId =
            PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), EnumValue->getName(),
                           EnumValue, D3D12_HLSL_NODE_TYPE_ENUM_VALUE, nodeId,
                           uint32_t(Refl.EnumValues.size()));

        Refl.EnumValues.push_back(
            {EnumValue->getInitVal().getSExtValue(), childNodeId});
      }

      if (Refl.EnumValues.size() >= uint32_t(1 << 30))
        throw std::invalid_argument("Enum values overflow");

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

    else if (FieldDecl *fieldDecl = dyn_cast<FieldDecl>(it)) {

      uint32_t typeId =
          GenerateTypeInfo(ASTCtx, Refl, fieldDecl->getType(), DefaultRowMaj);

      PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), fieldDecl->getName(),
                     fieldDecl, D3D12_HLSL_NODE_TYPE_VARIABLE, ParentNodeId,
                     typeId);
    }

    else if (ValueDecl *ValDecl = dyn_cast<ValueDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
        continue;

      VarDecl *varDecl = dyn_cast<VarDecl>(it);

      if (varDecl && varDecl->hasAttr<HLSLGroupSharedAttr>()) {

        if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
          continue;
      
        const std::string &name = ValDecl->getName();

        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, ValDecl->getType(), DefaultRowMaj);

        PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                       D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE, ParentNodeId,
                       typeId);

        continue;
      }

      if (varDecl && varDecl->getStorageClass() == StorageClass::SC_Static) {

        if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
          continue;

        const std::string &name = ValDecl->getName();

        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, ValDecl->getType(), DefaultRowMaj);

        PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                       D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE, ParentNodeId, typeId);

        continue;
      }

      uint32_t arraySize = 1;
      QualType type = ValDecl->getType();
      std::vector<uint32_t> arrayElem;

      while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(type)) {
        uint32_t current = arr->getSize().getZExtValue();
        arrayElem.push_back(current);
        arraySize *= arr->getSize().getZExtValue();
        type = arr->getElementType();
      }

      if (!IsHLSLResourceType(type)) {

        // Handle $Globals

        if (varDecl &&
            (Depth == 0 || Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)) {

          const std::string &name = ValDecl->getName();

          uint32_t typeId =
              GenerateTypeInfo(ASTCtx, Refl, ValDecl->getType(), DefaultRowMaj);

          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                         D3D12_HLSL_NODE_TYPE_VARIABLE, ParentNodeId, typeId);
        }

        continue;
      }

      if (Depth != 0)
        continue;

      FillReflectionRegisterAt(Ctx, ASTCtx, SM, Diags, type, arraySize, ValDecl,
                               arrayElem, Refl, AutoBindingSpace, ParentNodeId,
                               DefaultRowMaj);
    }

    else if (RecordDecl *RecDecl = dyn_cast<RecordDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      bool isDefinition = RecDecl->isThisDeclarationADefinition();

      D3D12_HLSL_NODE_TYPE type = D3D12_HLSL_NODE_TYPE_RESERVED;

      switch (RecDecl->getTagKind()) {

      case TTK_Struct:
        type = D3D12_HLSL_NODE_TYPE_STRUCT;
        break;

      case TTK_Union:
        type = D3D12_HLSL_NODE_TYPE_UNION;
        break;

      case TTK_Interface:
        type = D3D12_HLSL_NODE_TYPE_INTERFACE;
        break;
      }

      if (type != D3D12_HLSL_NODE_TYPE_RESERVED) {

        uint32_t typeId = 0;

        if (isDefinition)
          typeId = GenerateTypeInfo(
              ASTCtx, Refl, RecDecl->getASTContext().getRecordType(RecDecl),
              DefaultRowMaj);

        uint32_t self = PushNextNodeId(
            Refl, SM, ASTCtx.getLangOpts(), RecDecl->getName(), RecDecl, type,
            ParentNodeId, typeId, nullptr, &FwdDecls);

        if (self == uint32_t(-1)) // Duplicate fwd definition
          continue;

        if (isDefinition)
          RecursiveReflectHLSL(*RecDecl, ASTCtx, Diags, SM, Refl,
                               AutoBindingSpace, Depth + 1, Features, self,
                               DefaultRowMaj, FwdDecls);
      }
    }

    else if (NamespaceDecl *Namespace = dyn_cast<NamespaceDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES))
        continue;

      uint32_t nodeId = PushNextNodeId(
          Refl, SM, ASTCtx.getLangOpts(), Namespace->getName(), Namespace,
          D3D12_HLSL_NODE_TYPE_NAMESPACE, ParentNodeId, 0);

      RecursiveReflectHLSL(*Namespace, ASTCtx, Diags, SM, Refl,
                           AutoBindingSpace, Depth + 1, Features, nodeId,
                           DefaultRowMaj, FwdDecls);
    }
  }
}

bool DxcHLSLReflectionData::Initialize(clang::CompilerInstance &Compiler,
                                       clang::TranslationUnitDecl &Ctx,
                                       uint32_t AutoBindingSpace,
                                       D3D12_HLSL_REFLECTION_FEATURE Features,
                                       bool DefaultRowMaj,
                                       DxcHLSLReflectionData &Result) {

  DiagnosticsEngine &Diags = Ctx.getParentASTContext().getDiagnostics();
  const SourceManager &SM = Compiler.getSourceManager();

  Result = {};
  Result.Features = Features;

  if (Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) {
    Result.Strings.push_back("");
    Result.StringsToId[""] = 0;
    Result.NodeSymbols.push_back(DxcHLSLNodeSymbol(0, 0, 0, 0, 0, 0));
  }

  Result.Nodes.push_back(DxcHLSLNode{D3D12_HLSL_NODE_TYPE_NAMESPACE, false, 0,
                                     0, 0, 0xFFFF, 0, uint16_t(-1)});

  try {

    std::unordered_map<const Decl *, uint32_t> fwdDecls;
    RecursiveReflectHLSL(Ctx, Compiler.getASTContext(), Diags, SM, Result,
                         AutoBindingSpace, 0, Features, 0, DefaultRowMaj,
                         fwdDecls);
    return true;

  } catch (const std::invalid_argument &arg) {
    llvm::errs() << "DxcHLSLReflectionData::Initialize: Failed " << arg.what();
    return false;
  }
}

//TODO: Debug print code

static std::string RegisterGetArraySize(const DxcHLSLReflectionData &Refl, const DxcHLSLRegister &reg) {

  if (reg.ArrayId != (uint32_t)-1) {

    DxcHLSLArray arr = Refl.Arrays[reg.ArrayId];
    std::string str;

    for (uint32_t i = 0; i < arr.ArrayElem(); ++i)
      str += "[" + std::to_string(Refl.ArraySizes[arr.ArrayStart() + i]) + "]";

    return str;
  }

  return reg.BindCount > 1 ? "[" + std::to_string(reg.BindCount) + "]" : "";
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

  if (Type.Class != D3D_SVC_STRUCT && Type.Class != D3D_SVC_INTERFACE_CLASS) {

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

static std::string PrintTypeInfo(const DxcHLSLReflectionData &Refl,
                                 const DxcHLSLType &Type,
                                 const DxcHLSLTypeSymbol &Symbol,
                                 const std::string &PreviousTypeName) {

  std::string result = PrintArray(Refl, Type.UnderlyingArray);

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

  if (type.BaseClass != uint32_t(-1))
    RecursePrintType(Refl, type.BaseClass, Depth + 1,
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
             hasSymbols ? Refl.Strings[Refl.NodeSymbols[NodeId].NameId].c_str()
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

        if (reg.ArrayId == (uint32_t)-1 && reg.BindCount == 1)
          break;

        printf("%s%s\n", std::string(Depth, '\t').c_str(),
               RegisterGetArraySize(Refl, reg).c_str());
        break;
      }

      case D3D12_HLSL_NODE_TYPE_UNION:
      case D3D12_HLSL_NODE_TYPE_STRUCT: { // Children are Variables

        const DxcHLSLType &type = Refl.Types[localId];

        if (type.BaseClass != uint32_t(-1))
          RecursePrintType(Refl, type.BaseClass, Depth, "BaseClass ");

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
        const DxcHLSLNode &Node = Refl.Nodes[Stmt.NodeId];

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

  if (type.Class == D3D_SVC_STRUCT)
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

  std::string self = Refl.Strings[Refl.NodeSymbols[NodeId].NameId];

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

      if (type.Class == D3D_SVC_STRUCT)
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

    if (hasSymbolInfo && (NodeSymbols[i].NameId >= header.Strings ||
                          (NodeSymbols[i].FileSourceId != uint16_t(-1) &&
                           NodeSymbols[i].FileSourceId >= header.Sources)))
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

    if(
      reg.NodeId >= header.Nodes || 
      Nodes[reg.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_REGISTER ||
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

  for (uint32_t i = 0; i < header.Functions; ++i) {

    const DxcHLSLFunction &func = Functions[i];

    if (func.NodeId >= header.Nodes ||
        Nodes[func.NodeId].GetNodeType() != D3D12_HLSL_NODE_TYPE_FUNCTION ||
        Nodes[func.NodeId].GetLocalId() != i)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " points to an invalid nodeId");

    uint32_t paramCount = func.GetNumParameters() + func.HasReturn();

    if (Nodes[func.NodeId].GetChildCount() < paramCount)
      throw std::invalid_argument("Function " + std::to_string(i) +
                                  " is missing parameters and/or return");

    for (uint32_t j = 0; j < paramCount; ++j)
      if (Nodes[func.NodeId + 1 + j].GetParentId() != func.NodeId ||
          Nodes[func.NodeId + 1 + j].GetNodeType() !=
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

    if (Stmt.NodeId >= header.Nodes || Nodes[Stmt.NodeId].GetLocalId() != i)
      throw std::invalid_argument("Statement " + std::to_string(i) +
                                  " points to an invalid nodeId");

    bool condVar = Stmt.HasConditionVar();
    uint32_t minParamCount = Stmt.GetNodeCount() + condVar;
    const DxcHLSLNode &node = Nodes[Stmt.NodeId];

    if (node.GetChildCount() < minParamCount)
      throw std::invalid_argument("Statement " + std::to_string(i) +
                                  " didn't have required child nodes");

    if (condVar &&
        Nodes[Stmt.NodeId + 1].GetNodeType() != D3D12_HLSL_NODE_TYPE_VARIABLE)
      throw std::invalid_argument(
          "Statement " + std::to_string(i) +
          " has condition variable but first child is not a variable");

    switch (Nodes[Stmt.NodeId].GetNodeType()) {
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

    if ((type.BaseClass != uint32_t(-1) && type.BaseClass >= header.Types) ||
        type.GetMemberStart() + type.GetMemberCount() > header.Members ||
        type.GetInterfaceStart() + type.GetInterfaceCount() >
            header.TypeListCount ||
        (type.UnderlyingArray.ElementsOrArrayId >> 31 &&
         (type.UnderlyingArray.ElementsOrArrayId << 1 >> 1) >= header.Arrays))
      throw std::invalid_argument(
          "Type " + std::to_string(i) +
          " points to an invalid string, array, base class or member");

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

        if (!type.GetMemberCount())
          throw std::invalid_argument("Type (struct) " + std::to_string(i) +
                                      " is missing children");

        [[fallthrough]];

    case D3D_SVC_INTERFACE_CLASS:

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

  //Validate fwd & backwards declares

   for (uint32_t i = 0; i < header.Nodes; ++i) {

    const DxcHLSLNode &node = Nodes[i];

    if (node.IsFwdBckDefined()) {

      uint32_t fwdBack = node.GetFwdBck();

      if (Nodes[fwdBack].GetNodeType() != node.GetNodeType())
          throw std::invalid_argument(
              "Node " + std::to_string(i) +
              " (fwd/bck declare) points to element that of incompatible type");

      if (hasSymbolInfo && NodeSymbols[fwdBack].NameId != NodeSymbols[i].NameId)
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

}
