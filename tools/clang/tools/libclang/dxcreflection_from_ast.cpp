///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxreflection_from_ast.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Handles walking the AST and turning it into a reflection object.          //
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

using namespace clang;

namespace hlsl {
    
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
            Refl.RegisterString(annotate->getAnnotation().str(), true), false));

        if (annotationCount >= uint8_t(-1))
          throw std::invalid_argument("Annotation count out of bounds");

        ++annotationCount;

      } else if (const HLSLShaderAttr *shaderAttr =
                     dyn_cast<HLSLShaderAttr>(attr)) {

        if (Refl.Annotations.size() >= (1u << 16))
          throw std::invalid_argument("Out of annotations");

        Refl.Annotations.push_back(DxcHLSLAnnotation(
            Refl.RegisterString(
                "shader(\"" + shaderAttr->getStage().str() + "\")", true),
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
          semanticId = Refl.RegisterString(
              cast<hlsl::SemanticDecl>(*It)->SemanticName.str(), true);
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
          Refl.Sources.push_back(Refl.RegisterString(fileName, false));
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

    uint32_t nameId = Refl.RegisterString(UnqualifiedName, false);

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

struct DxcRegisterTypeInfo {
  D3D_SHADER_INPUT_TYPE RegisterType;
  D3D_SHADER_INPUT_FLAGS RegisterFlags;
  D3D_SRV_DIMENSION TextureDimension;
  D3D_RESOURCE_RETURN_TYPE TextureValue;
};

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

  uint32_t arrayIdUnderlying = Refl.PushArray(arraySizeUnderlying, arrayElemUnderlying);
  DxcHLSLArrayOrElements elementsOrArrayIdUnderlying(arrayIdUnderlying, arraySizeUnderlying);

  uint32_t arrayIdDisplay = Refl.PushArray(arraySizeDisplay, arrayElemDisplay);
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

      uint32_t nameId = hasSymbols ? Refl.RegisterString(innerTypeName, false)
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
            hasSymbols ? Refl.RegisterString(name, false) : uint32_t(-1);

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
  Refl.RegisterTypeList(interfaces, interfaceOffset, interfaceCount);

  DxcHLSLType hlslType(baseType, elementsOrArrayIdUnderlying, cls, type, rows, columns,
                       membersCount, membersOffset, interfaceOffset,
                       interfaceCount);

  uint32_t displayNameId =
      hasSymbols ? Refl.RegisterString(displayName, false) : uint32_t(-1);

  uint32_t underlyingNameId =
      hasSymbols ? Refl.RegisterString(underlyingName, false) : uint32_t(-1);

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

  uint32_t arrayId = Refl.PushArray(ArraySizeFlat, ArraySize);

  uint32_t bufferId = 0;
  D3D_CBUFFER_TYPE bufferType =
      DxcHLSLReflectionData::GetBufferType(inputType.RegisterType);
  
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

  while (const AttributedStmt *AS = dyn_cast<AttributedStmt>(Statement))
    Statement = AS->getSubStmt();

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

  // Traverse AST to grab reflection data

  for (Decl *it : Ctx.decls()) {

    SourceLocation Loc = it->getLocation();
    if (Loc.isInvalid() || SM.isInSystemHeader(Loc))    //TODO: We might want to include these for a more complete picture.
      continue;

    if (isa<ParmVarDecl>(it))    //Skip parameters, already handled explicitly
      continue;

    if (HLSLBufferDecl *cbuffer = dyn_cast<HLSLBufferDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
        continue;

      if (Depth != 0)
        continue;

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), cbuffer->getName(),
                         cbuffer, D3D12_HLSL_NODE_TYPE_REGISTER, ParentNodeId,
                         uint32_t(Refl.Registers.size()));

      uint32_t bufferId = RegisterBuffer(ASTCtx, Refl, SM, cbuffer, nodeId,
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

    else if (FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS))
        continue;

      if (funcDecl->isImplicit())   //Skip ctors, etc.
        continue;

      const FunctionDecl *definition = nullptr;

      uint32_t nodeId =
          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), funcDecl->getName(), funcDecl,
                         D3D12_HLSL_NODE_TYPE_FUNCTION, ParentNodeId,
                         uint32_t(Refl.Functions.size()), nullptr, &FwdDecls);

      if (nodeId == uint32_t(-1)) // Duplicate fwd definition
        continue;

      bool hasDefinition = funcDecl->hasBody(definition);
      DxcHLSLFunction func = {nodeId, funcDecl->getNumParams(),
                              !funcDecl->getReturnType().getTypePtr()->isVoidType(),
                              hasDefinition};

      for (uint32_t i = 0; i < func.GetNumParameters(); ++i)
        AddFunctionParameter(ASTCtx, funcDecl->getParamDecl(i)->getType(),
                             funcDecl->getParamDecl(i), Refl, SM, nodeId,
                             DefaultRowMaj);

      if (func.HasReturn())
        AddFunctionParameter(ASTCtx, funcDecl->getReturnType(), nullptr, Refl, SM,
                             nodeId, DefaultRowMaj);

      Refl.Functions.push_back(std::move(func));

      if (hasDefinition && (Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)) {

        Stmt *stmt = funcDecl->getBody();

        for (const Stmt *subStmt : stmt->children()) {

          if (!subStmt)
            continue;

          RecursiveReflectBody(subStmt, ASTCtx, Diags, SM, Refl,
                               AutoBindingSpace, Depth, Features, nodeId,
                               DefaultRowMaj, FwdDecls, ASTCtx.getLangOpts());
        }
      }
    }

    else if (TypedefDecl *typedefDecl = dyn_cast<TypedefDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      uint32_t typeId = GenerateTypeInfo(
          ASTCtx, Refl, typedefDecl->getUnderlyingType(), DefaultRowMaj);

      PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), typedefDecl->getName(),
                     typedefDecl, D3D12_HLSL_NODE_TYPE_TYPEDEF, ParentNodeId,
                     typeId);
    }

    else if (TypeAliasDecl *typeAlias = dyn_cast<TypeAliasDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      // TODO: Implement. typeAlias->print(pfStream, printingPolicy);
    }

    else if (EnumDecl *enumDecl = dyn_cast<EnumDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      uint32_t nodeId = PushNextNodeId(
          Refl, SM, ASTCtx.getLangOpts(), enumDecl->getName(), enumDecl,
                         D3D12_HLSL_NODE_TYPE_ENUM, ParentNodeId,
                         uint32_t(Refl.Enums.size()), nullptr, &FwdDecls);

      if (nodeId == uint32_t(-1)) // Duplicate fwd definition
        continue;

      for (EnumConstantDecl *enumValue : enumDecl->enumerators()) {

        uint32_t childNodeId =
            PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), enumValue->getName(),
                           enumValue, D3D12_HLSL_NODE_TYPE_ENUM_VALUE, nodeId,
                           uint32_t(Refl.EnumValues.size()));

        Refl.EnumValues.push_back(
            {enumValue->getInitVal().getSExtValue(), childNodeId});
      }

      if (Refl.EnumValues.size() >= uint32_t(1 << 30))
        throw std::invalid_argument("Enum values overflow");

      QualType enumType = enumDecl->getIntegerType();
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

    else if (ValueDecl *valDecl = dyn_cast<ValueDecl>(it)) {

      VarDecl *varDecl = dyn_cast<VarDecl>(it);

      if (varDecl && varDecl->hasAttr<HLSLGroupSharedAttr>()) {

        if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
          continue;
      
        const std::string &name = valDecl->getName();

        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, valDecl->getType(), DefaultRowMaj);

        PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                       D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE, ParentNodeId,
                       typeId);

        continue;
      }

      if (varDecl && varDecl->getStorageClass() == StorageClass::SC_Static) {

        if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
          continue;

        const std::string &name = valDecl->getName();

        uint32_t typeId =
            GenerateTypeInfo(ASTCtx, Refl, valDecl->getType(), DefaultRowMaj);

        PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                       D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE, ParentNodeId, typeId);

        continue;
      }

      uint32_t arraySize = 1;
      QualType type = valDecl->getType();
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

          const std::string &name = valDecl->getName();

          uint32_t typeId =
              GenerateTypeInfo(ASTCtx, Refl, valDecl->getType(), DefaultRowMaj);

          PushNextNodeId(Refl, SM, ASTCtx.getLangOpts(), name, it,
                         D3D12_HLSL_NODE_TYPE_VARIABLE, ParentNodeId, typeId);
        }

        continue;
      }

      if (Depth != 0)
        continue;

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
        continue;

      FillReflectionRegisterAt(Ctx, ASTCtx, SM, Diags, type, arraySize, valDecl,
                               arrayElem, Refl, AutoBindingSpace, ParentNodeId,
                               DefaultRowMaj);
    }

    else if (RecordDecl *recDecl = dyn_cast<RecordDecl>(it)) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        continue;

      bool isDefinition = recDecl->isThisDeclarationADefinition();

      D3D12_HLSL_NODE_TYPE type = D3D12_HLSL_NODE_TYPE_RESERVED;

      switch (recDecl->getTagKind()) {

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
              ASTCtx, Refl, recDecl->getASTContext().getRecordType(recDecl),
              DefaultRowMaj);

        uint32_t self = PushNextNodeId(
            Refl, SM, ASTCtx.getLangOpts(), recDecl->getName(), recDecl, type,
            ParentNodeId, typeId, nullptr, &FwdDecls);

        if (self == uint32_t(-1)) // Duplicate fwd definition
          continue;

        if (isDefinition)
          RecursiveReflectHLSL(*recDecl, ASTCtx, Diags, SM, Refl,
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

bool DxcHLSLReflectionDataFromAST(DxcHLSLReflectionData &Result,
                                  CompilerInstance &Compiler,
                                  TranslationUnitDecl &Ctx,
                                  uint32_t AutoBindingSpace,
                                  D3D12_HLSL_REFLECTION_FEATURE Features,
                                  bool DefaultRowMaj) {

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

} // namespace hlsl
