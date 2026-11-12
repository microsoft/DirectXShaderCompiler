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

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"

#include "dxc/DxcReflection/DxcReflectionContainer.h"

using namespace clang;

namespace hlsl {

// DxilInterpolationMode.cpp but a little bit cleaned up
static D3D_INTERPOLATION_MODE GetInterpolationMode(const Decl *decl) {

  if (!decl) // Return type
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
      D3D_INTERPOLATION_CONSTANT};

  return modes[mask];
}

[[nodiscard]] static ReflectionError
PushNextNodeId(uint32_t &NodeId, ReflectionData &Refl, const SourceManager &SM,
               const LangOptions &LangOpts, const std::string &UnqualifiedName,
               const Decl *DeclSelf, D3D12_HLSL_NODE_TYPE Type,
               uint32_t ParentNodeId, uint32_t LocalId,
               const SourceRange *Range = nullptr,
               std::unordered_map<const Decl *, uint32_t> *FwdDecls = nullptr) {

  if (Refl.Nodes.size() >= (1u << 24))
    return HLSL_REFL_ERR("Nodes overflow");

  if (LocalId >= (1u << 24))
    return HLSL_REFL_ERR("LocalId overflow");

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

      if (isFwdDeclare && rec->isImplicit()) { // Inner ghost node
        NodeId = uint32_t(-1);
        return ReflectionErrorSuccess;
      }
    }
  }

  //There is a forward declare, but we haven't seen it before.
  //This happens for example if we have a fwd func declare in a struct, but define it in global namespace.
  //(only) -reflect-function will hide this struct from us, but will still find a function in the global scope.
  //This fixes that problem.

  if (!isFwdDeclare && fwdDeclare && fwdDeclare != DeclSelf &&
      FwdDecls->find(fwdDeclare) == FwdDecls->end()) {
    NodeId = uint32_t(-1);
    return ReflectionErrorSuccess;
  }

  uint32_t nodeId = Refl.Nodes.size();

  uint16_t annotationStart = uint16_t(Refl.Annotations.size());
  uint8_t annotationCount = 0;

  uint16_t semanticId = uint16_t(-1);

  if (DeclSelf) {
    for (const Attr *attr : DeclSelf->attrs()) {
      if (const AnnotateAttr *annotate = dyn_cast<AnnotateAttr>(attr)) {

        if (Refl.Annotations.size() >= (1u << 16))
          return HLSL_REFL_ERR("Out of annotations");

        Refl.Annotations.push_back({});

        uint32_t stringId;

        if (ReflectionError err = Refl.RegisterString(
                stringId, annotate->getAnnotation().str(), true))
          return err;

        if (ReflectionError err = ReflectionAnnotation::Initialize(
                Refl.Annotations.back(), stringId, false))
          return err;

        if (annotationCount >= uint8_t(-1))
          return HLSL_REFL_ERR("Annotation count out of bounds");

        ++annotationCount;

      } else if (const HLSLShaderAttr *shaderAttr =
                     dyn_cast<HLSLShaderAttr>(attr)) {

        if (Refl.Annotations.size() >= (1u << 16))
          return HLSL_REFL_ERR("Out of annotations");

        Refl.Annotations.push_back({});

        uint32_t stringId;

        if (ReflectionError err = Refl.RegisterString(
                stringId, "shader(\"" + shaderAttr->getStage().str() + "\")",
                true))
          return err;

        if (ReflectionError err = ReflectionAnnotation::Initialize(
                Refl.Annotations.back(), stringId, true))
          return err;

        if (annotationCount >= uint8_t(-1))
          return HLSL_REFL_ERR("Annotation count out of bounds");

        ++annotationCount;
      }

      // TODO: Other types of attrs
    }

    if (const ValueDecl *valDecl = dyn_cast<ValueDecl>(DeclSelf)) {

      const ArrayRef<hlsl::UnusualAnnotation *> &UA =
          valDecl->getUnusualAnnotations();

      for (auto It = UA.begin(), E = UA.end(); It != E; ++It)
        if ((*It)->getKind() == hlsl::UnusualAnnotation::UA_SemanticDecl) {

          uint32_t semanticId32;

          if (ReflectionError err = Refl.RegisterString(
                  semanticId32,
                  cast<hlsl::SemanticDecl>(*It)->SemanticName.str(), true))
            return err;

          semanticId = uint16_t(semanticId32);
          break;
        }
    }
  }

  D3D_INTERPOLATION_MODE interpolationMode = GetInterpolationMode(DeclSelf);

  uint32_t currId = uint32_t(Refl.Nodes.size());

  if (canHaveFwdDeclare) {

    assert(FwdDecls && "Fwd decl requires FwdDecls map to be present");

    // Multiple fwd declare, ignore
    if (isFwdDeclare && FwdDecls->find(fwdDeclare) != FwdDecls->end()) {
      NodeId = uint32_t(-1);
      return ReflectionErrorSuccess;
    }

    if (isFwdDeclare)
      (*FwdDecls)[fwdDeclare] = currId;
  }

  Refl.Nodes.push_back({});
  if (ReflectionError err = ReflectionNode::Initialize(
          Refl.Nodes.back(), Type, isFwdDeclare, LocalId, annotationStart, 0,
          ParentNodeId, annotationCount, semanticId, interpolationMode))
    return err;

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
          return HLSL_REFL_ERR("End and start are not in the same file");

        auto it = Refl.StringToSourceId.find(fileName);
        uint32_t i;

        if (it == Refl.StringToSourceId.end()) {

          i = uint32_t(Refl.Sources.size());
          uint32_t stringId;

          if (ReflectionError err =
                  Refl.RegisterString(stringId, fileName, false))
            return err;

          Refl.Sources.push_back(stringId);
          Refl.StringToSourceId[fileName] = i;
        }

        else {
          i = it->second;
        }

        if (i >= 65535)
          return HLSL_REFL_ERR("Source file count is limited to 16-bit");

        if ((endLine - startLine) >= 65535)
          return HLSL_REFL_ERR("Source line count is limited to 16-bit");

        if (startLine >= 1048576)
          return HLSL_REFL_ERR("Source line start is limited to 20-bit");

        if (startCol >= (1u << 22))
          return HLSL_REFL_ERR("Column start is limited to 22-bit");

        if (endCol >= (1u << 22))
          return HLSL_REFL_ERR("Column end is limited to 22-bit");

        sourceLineCount = uint16_t(endLine - startLine + 1);
        sourceLineStart = startLine;
        sourceColumnStart = startCol;
        sourceColumnEnd = endCol;
        sourceId = uint16_t(i);
      }
    }

    uint32_t nameId;

    if (ReflectionError err =
            Refl.RegisterString(nameId, UnqualifiedName, false))
      return err;

    Refl.NodeSymbols.push_back({});

    if (ReflectionError err = ReflectionNodeSymbol::Initialize(
            Refl.NodeSymbols.back(), nameId, sourceId, sourceLineCount,
            sourceLineStart, sourceColumnStart, sourceColumnEnd))
      return err;
  }

  // Link

  if (DeclSelf && fwdDeclare != DeclSelf && fwdDeclare && !isFwdDeclare) {
    assert(FwdDecls &&
           "Referencing fwd decl requires FwdDecls map to be present");
    uint32_t fwd = (*FwdDecls)[fwdDeclare];

    if (ReflectionError err =
            Refl.Nodes[fwd].ResolveFwdDeclare(fwd, Refl.Nodes[currId], currId))
      return err;
  }

  uint32_t parentParent = ParentNodeId;

  while (parentParent != 0) {

    ReflectionNode &parent = Refl.Nodes[parentParent];

    if (ReflectionError err = parent.IncreaseChildCount())
      return err;

    parentParent = parent.GetParentId();
  }

  if (ReflectionError err = Refl.Nodes[0].IncreaseChildCount())
    return err;

  NodeId = nodeId;
  return ReflectionErrorSuccess;
}

struct DxcRegisterTypeInfo {
  D3D_SHADER_INPUT_TYPE RegisterType;
  D3D_SHADER_INPUT_FLAGS RegisterFlags;
  D3D_SRV_DIMENSION TextureDimension;
  D3D_RESOURCE_RETURN_TYPE TextureValue;
};

static DxcRegisterTypeInfo
GetTextureRegisterInfo(ASTContext &ASTCtx, std::string TypeName, bool IsWrite,
                       const CXXRecordDecl *RecordDecl) {

  DxcRegisterTypeInfo type = {};
  type.RegisterType = IsWrite ? D3D_SIT_UAV_RWTYPED : D3D_SIT_TEXTURE;

  // Parse return type and dimensions

  const ClassTemplateSpecializationDecl *textureTemplate =
      dyn_cast<ClassTemplateSpecializationDecl>(RecordDecl);

  assert(textureTemplate && "Expected texture template");

  const ArrayRef<TemplateArgument> &textureParams =
      textureTemplate->getTemplateArgs().asArray();

  bool shouldBeTexture2DMS = textureParams.size() == 2;

  if (shouldBeTexture2DMS)
    assert(textureParams[0].getKind() == TemplateArgument::Type &&
           textureParams[1].getKind() == TemplateArgument::Integral &&
           "Expected template args");

  else
    assert(textureParams.size() == 1 &&
           textureParams[0].getKind() == TemplateArgument::Type &&
           "Expected template args");

  QualType valueType = textureParams[0].getAsType();
  QualType desugared = valueType.getDesugaredType(ASTCtx);

  uint32_t dimensions;

  if (isa<BuiltinType>(desugared))
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

  // Parse type

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

  if (isArray) // Arrays are always 1 behind the regular type
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
    info.RegisterType = isWrite ? D3D_SIT_UAV_RWSTRUCTURED : D3D_SIT_STRUCTURED;
    return info;
  }

  if (typeName == "ByteAddressBuffer") {
    info.RegisterType =
        isWrite ? D3D_SIT_UAV_RWBYTEADDRESS : D3D_SIT_BYTEADDRESS;
    return info;
  }

  return GetTextureRegisterInfo(ASTCtx, typeName, isWrite, recordDecl);
}

// Collect array sizes in the logical order (inner dims first, then outer).
// Example:
//   F32[4][3]         -> {4, 3}
//   typedef T = F32[4][3];
//   T[2]              -> {4, 3, 2}
[[nodiscard]] static ReflectionError
CollectUnderlyingArraySizes(QualType &T, std::vector<uint32_t> &Out,
                            uint64_t &FlatSize) {

  T = T.getNonReferenceType();

  std::vector<uint32_t> local;

  while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(T)) {

    uint64_t siz = arr->getSize().getZExtValue();
    FlatSize *= siz;

    if ((FlatSize >> 32) || (siz >> 32))
      return HLSL_REFL_ERR("Can't calculate flat array size: out of bits");

    local.push_back(uint32_t(siz));
    T = arr->getElementType().getNonReferenceType();
  }

  if (const TypedefType *td = dyn_cast<TypedefType>(T)) {

    T = td->getDecl()->getUnderlyingType().getNonReferenceType();

    if (T->isArrayType())
      if (ReflectionError err = CollectUnderlyingArraySizes(T, Out, FlatSize))
        return err;
  }

  if (const TemplateSpecializationType *ts =
          dyn_cast<TemplateSpecializationType>(T)) {
    QualType desugared = ts->desugar().getNonReferenceType();
    if (desugared != T) {
      T = desugared;
      if (ReflectionError err = CollectUnderlyingArraySizes(T, Out, FlatSize))
        return err;
    }
  }

  Out.insert(Out.end(), local.begin(), local.end());
  return ReflectionErrorSuccess;
}

[[nodiscard]] ReflectionError
GenerateTypeInfo(uint32_t &TypeId, ASTContext &ASTCtx, ReflectionData &Refl,
                 QualType Original, bool DefaultRowMaj) {

  // Unwrap array
  // There's the following issue:
  // Let's say the underlying type is F32x4[4] but the sugared name is F32x4x4,
  //  then we want to maintain sugared name + array info (of sugar) for
  //  reflection but for low level type info, we would want to know float4[4]

  uint64_t arraySizeUnderlying = 1;
  std::vector<uint32_t> arrayElemUnderlying;

  QualType underlying = Original;
  if (ReflectionError err = CollectUnderlyingArraySizes(
          underlying, arrayElemUnderlying, arraySizeUnderlying))
    return err;

  uint32_t arraySizeDisplay = 1;
  std::vector<uint32_t> arrayElemDisplay;
  QualType display = Original.getNonReferenceType();

  while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(display)) {
    uint32_t current = arr->getSize().getZExtValue();
    arrayElemDisplay.push_back(current);
    arraySizeDisplay *= arr->getSize().getZExtValue();
    display = arr->getElementType().getNonReferenceType();
  }

  // Unwrap enum in underlying to still treat it as a uint/int/etc.

  if (const EnumType *enumTy = dyn_cast<EnumType>(underlying)) {

    const EnumDecl *decl = enumTy->getDecl();

    if (decl && !decl->getIntegerType().isNull())
      underlying =
          decl->getIntegerType().getNonReferenceType().getCanonicalType();

    else
      underlying = ASTCtx.IntTy;
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

  // Prune template instantiation from type name for builtin types (ex. vector &
  // matrix) But only if it's not a sugared type:
  //  typedef ConstantBuffer<Test> MyTest;
  // In this case, MyTest will still be seen as a ConstantBuffer<Test> but the
  // typeName is MyTest.

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

        if (it->second >= 0)
          displayName = underlyingName = it->first;
      }
    }
  }

  bool hasSymbols = Refl.Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO;

  // Two arrays; for display and for underlying

  uint32_t arrayIdUnderlying;

  if (ReflectionError err = Refl.PushArray(
          arrayIdUnderlying, arraySizeUnderlying, arrayElemUnderlying))
    return err;

  ReflectionArrayOrElements elementsOrArrayIdUnderlying(arrayIdUnderlying,
                                                        arraySizeUnderlying);

  uint32_t arrayIdDisplay;
  if (ReflectionError err =
          Refl.PushArray(arrayIdDisplay, arraySizeDisplay, arrayElemDisplay))
    return err;

  ReflectionArrayOrElements elementsOrArrayIdDisplay(arrayIdDisplay,
                                                     arraySizeDisplay);

  // Unwrap vector and matrix
  // And base type

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
    std::string innerTypeName; //$Element or T depending on type

    if (const ClassTemplateSpecializationDecl *templateClass =
            dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {

      std::string name = templateClass->getIdentifier()->getName();

      const ArrayRef<TemplateArgument> &params =
          templateClass->getTemplateArgs().asArray();

      auto it = lookup.find(name);

      if (it != lookup.end()) {

        D3D_SHADER_VARIABLE_TYPE svt = it->second;

        if (svt == -1) { // Reserved as 'vector'

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

        else if (svt == -2) { // Reserved as 'matrix'

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
          underlyingName = name;

          innerTypeName = "$Element";

          const TemplateSpecializationType *templateDesc =
              display->getAs<TemplateSpecializationType>();

          // Case 1: T = StructuredBuffer<T> then underlying has a type, but
          // sugared doesn't. Loses syntax sugar, but will still be correct.

          bool useUnderlying = !templateDesc;

          // Case 2: TextureCube = TextureCube<T = float4>

          if (templateDesc && displayName == underlyingName &&
              !templateDesc->getNumArgs())
            useUnderlying = true;

          if (useUnderlying) {

            assert(params.size() &&
                   "Expected a TemplateSpecializationType with > 0 args");

            innerType = params[0].getAsType();

            if (svt == D3D_SVT_RWBUFFER || svt == D3D_SVT_BUFFER ||
                svt == D3D_SVT_CBUFFER)
              innerTypeName =
                  innerType.getUnqualifiedType().getAsString(policy);

          } else {

            assert(templateDesc &&
                   "Expected a valid TemplateSpecializationType");
            assert(templateDesc->getNumArgs() &&
                   "Expected a TemplateSpecializationType with > 0 args");

            innerType = templateDesc->getArg(0).getAsType();

            if (svt == D3D_SVT_RWBUFFER || svt == D3D_SVT_BUFFER ||
                svt == D3D_SVT_CBUFFER)
              innerTypeName =
                  innerType.getUnqualifiedType().getAsString(policy);
          }
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

      uint32_t nameId = uint32_t(-1);

      if (hasSymbols)
        if (ReflectionError err =
                Refl.RegisterString(nameId, innerTypeName, false))
          return err;

      uint32_t typeId;

      if (ReflectionError err =
              GenerateTypeInfo(typeId, ASTCtx, Refl, innerType, DefaultRowMaj))
        return err;

      membersOffset = uint32_t(Refl.MemberTypeIds.size());
      membersCount = 1;

      Refl.MemberTypeIds.push_back(typeId);

      if (hasSymbols)
        Refl.MemberNameIds.push_back(nameId);
    }

    // Fill members

    if (!standardType && recordDecl->isCompleteDefinition() &&
        cls != D3D_SVC_OBJECT) {

      // Base types

      if (CXXRecordDecl *cxxRecordDecl = dyn_cast<CXXRecordDecl>(recordDecl))
        if (cxxRecordDecl->getNumBases()) {
          for (auto &I : cxxRecordDecl->bases()) {

            QualType qualType = I.getType();
            CXXRecordDecl *BaseDecl =
                cast<CXXRecordDecl>(qualType->castAs<RecordType>()->getDecl());

            if (BaseDecl->isInterface()) {

              uint32_t interfaceId;

              if (ReflectionError err = GenerateTypeInfo(
                      interfaceId, ASTCtx, Refl, qualType, DefaultRowMaj))
                return err;

              interfaces.push_back(interfaceId);
              continue;
            }

            assert(baseType == uint32_t(-1) &&
                   "Multiple base types isn't supported in HLSL");

            if (ReflectionError err = GenerateTypeInfo(baseType, ASTCtx, Refl,
                                                       qualType, DefaultRowMaj))
              return err;
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

        uint32_t nameId = uint32_t(-1);

        if (hasSymbols) {

          if (ReflectionError err = Refl.RegisterString(nameId, name, false))
            return err;

          Refl.MemberNameIds.push_back(nameId);
        }

        ++membersCount;
      }

      if (membersCount) {

        Refl.MemberTypeIds.resize(Refl.MemberTypeIds.size() + membersCount);

        if (Refl.MemberTypeIds.size() >= uint32_t(1u << 24))
          return HLSL_REFL_ERR("Members out of bounds");
      }

      // Initialize member types (because it causes recursion)

      membersCount = 0;

      for (Decl *decl : recordDecl->decls()) {

        FieldDecl *fieldDecl = dyn_cast<FieldDecl>(decl);

        if (!fieldDecl)
          continue;

        uint32_t memberTypeId;
        if (ReflectionError err =
                GenerateTypeInfo(memberTypeId, ASTCtx, Refl,
                                 fieldDecl->getType(), DefaultRowMaj))
          return err;

        Refl.MemberTypeIds[membersOffset + membersCount] = memberTypeId;

        ++membersCount;
      }
    }
  }

  // Type name

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
      underlyingName = "float16_t"; // TODO: half or float16_t?
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
      return HLSL_REFL_ERR("Invalid builtin type");
    }
  }

  // Turn into proper fully qualified name (e.g. turn vector<float, 4> into
  // float4)

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

  // Insert

  if (Refl.Types.size() >= uint32_t(-1))
    return HLSL_REFL_ERR("Type id out of bounds");

  if (interfaces.size() >= uint8_t(-1))
    return HLSL_REFL_ERR("Only allowing 256 interfaces");

  uint32_t interfaceOffset = 0;
  uint8_t interfaceCount = 0;
  if (ReflectionError err =
          Refl.RegisterTypeList(interfaces, interfaceOffset, interfaceCount))
    return err;

  ReflectionVariableType hlslType;
  if (ReflectionError err = ReflectionVariableType::Initialize(
          hlslType, baseType, elementsOrArrayIdUnderlying, cls, type, rows,
          columns, membersCount, membersOffset, interfaceOffset,
          interfaceCount))
    return err;

  uint32_t displayNameId = uint32_t(-1);
  uint32_t underlyingNameId = uint32_t(-1);

  if (hasSymbols) {

    if (ReflectionError err =
            Refl.RegisterString(displayNameId, displayName, false))
      return err;

    if (ReflectionError err =
            Refl.RegisterString(underlyingNameId, underlyingName, false))
      return err;
  }

  ReflectionVariableTypeSymbol typeSymbol(elementsOrArrayIdDisplay,
                                          displayNameId, underlyingNameId);

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

  TypeId = i;
  return ReflectionErrorSuccess;
}

[[nodiscard]] static ReflectionError FillReflectionRegisterAt(
    const DeclContext &Ctx, ASTContext &ASTCtx, const SourceManager &SM,
    DiagnosticsEngine &Diag, QualType Type, uint32_t ArraySizeFlat,
    ValueDecl *ValDesc, const std::vector<uint32_t> &ArraySize,
    ReflectionData &Refl, uint32_t AutoBindingSpace, uint32_t ParentNodeId,
    bool DefaultRowMaj) {

  DxcRegisterTypeInfo inputType = GetRegisterTypeInfo(ASTCtx, Type);

  uint32_t nodeId;
  if (ReflectionError err = PushNextNodeId(
          nodeId, Refl, SM, ASTCtx.getLangOpts(), ValDesc->getName(), ValDesc,
          D3D12_HLSL_NODE_TYPE_REGISTER, ParentNodeId,
          uint32_t(Refl.Registers.size())))
    return err;

  uint32_t arrayId;

  if (ReflectionError err = Refl.PushArray(arrayId, ArraySizeFlat, ArraySize))
    return err;

  uint32_t bufferId = 0;
  D3D_CBUFFER_TYPE bufferType =
      ReflectionData::GetBufferType(inputType.RegisterType);

  if (bufferType != D3D_CT_INTERFACE_POINTERS) {
    bufferId = uint32_t(Refl.Buffers.size());
    Refl.Buffers.push_back({bufferType, nodeId});
  }

  ReflectionShaderResource regD3D12;

  if (ReflectionError err = ReflectionShaderResource::Initialize(
          regD3D12, inputType.RegisterType, ArraySizeFlat,
          uint32_t(inputType.RegisterFlags), inputType.TextureValue,
          inputType.TextureDimension, nodeId, arrayId, bufferId))
    return err;

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

    uint32_t typeId;

    if (ReflectionError err =
            GenerateTypeInfo(typeId, ASTCtx, Refl, innerType, DefaultRowMaj))
      return err;

    SourceRange sourceRange = ValDesc->getSourceRange();

    if (ReflectionError err = PushNextNodeId(
            nodeId, Refl, SM, ASTCtx.getLangOpts(),
            isListType ? "$Element" : ValDesc->getName(), nullptr,
            D3D12_HLSL_NODE_TYPE_VARIABLE, nodeId, typeId, &sourceRange))
      return err;

    break;
  }
  }

  return ReflectionErrorSuccess;
}

template <typename T>
[[nodiscard]] ReflectionError
RecurseBuffer(ASTContext &ASTCtx, const SourceManager &SM, ReflectionData &Refl,
              const T &Decls, bool DefaultRowMaj, uint32_t ParentId) {

  for (Decl *decl : Decls) {

    ValueDecl *valDecl = dyn_cast<ValueDecl>(decl);
    assert(valDecl && "Decl was expected to be a ValueDecl but wasn't");
    QualType original = valDecl->getType();

    const std::string &name = valDecl->getName();

    uint32_t typeId;
    if (ReflectionError err =
            GenerateTypeInfo(typeId, ASTCtx, Refl, original, DefaultRowMaj))
      return err;

    uint32_t nodeId;

    if (ReflectionError err =
            PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(), name, decl,
                           D3D12_HLSL_NODE_TYPE_VARIABLE, ParentId, typeId))
      return err;

    // Handle struct recursion

    if (RecordDecl *recordDecl = dyn_cast<RecordDecl>(decl)) {

      if (!recordDecl->isCompleteDefinition())
        continue;

      if (ReflectionError err = RecurseBuffer(
              ASTCtx, SM, Refl, recordDecl->fields(), DefaultRowMaj, nodeId))
        return err;
    }
  }

  return ReflectionErrorSuccess;
}

[[nodiscard]] ReflectionError
RegisterBuffer(uint32_t &bufferId, ASTContext &ASTCtx, ReflectionData &Refl,
               const SourceManager &SM, DeclContext *Buffer, uint32_t NodeId,
               D3D_CBUFFER_TYPE Type, bool DefaultRowMaj) {

  if (Refl.Buffers.size() >= uint32_t(-1))
    return HLSL_REFL_ERR("Buffer id out of bounds");

  bufferId = uint32_t(Refl.Buffers.size());

  if (ReflectionError err = RecurseBuffer(ASTCtx, SM, Refl, Buffer->decls(),
                                          DefaultRowMaj, NodeId))
    return err;

  Refl.Buffers.push_back({Type, NodeId});

  return ReflectionErrorSuccess;
}

[[nodiscard]] static ReflectionError
AddFunctionParameter(ASTContext &ASTCtx, QualType Type, Decl *Decl,
                     ReflectionData &Refl, const SourceManager &SM,
                     uint32_t ParentNodeId, bool DefaultRowMaj) {

  uint32_t typeId;

  if (ReflectionError err =
          GenerateTypeInfo(typeId, ASTCtx, Refl, Type, DefaultRowMaj))
    return err;

  uint32_t nodeId;

  if (ReflectionError err =
          PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(),
                         Decl && dyn_cast<NamedDecl>(Decl)
                             ? dyn_cast<NamedDecl>(Decl)->getName()
                             : "",
                         Decl, D3D12_HLSL_NODE_TYPE_PARAMETER, ParentNodeId,
                         uint32_t(Refl.Parameters.size())))
    return err;

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
      ReflectionFunctionParameter{typeId, nodeId, uint8_t(flags)});

  return ReflectionErrorSuccess;
}

[[nodiscard]] static ReflectionError RecursiveReflectBody(
    Stmt *Statement, ASTContext &ASTCtx, DiagnosticsEngine &Diags,
    const SourceManager &SM, ReflectionData &Refl, uint32_t AutoBindingSpace,
    uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
    uint32_t ParentNodeId, bool DefaultRowMaj,
    std::unordered_map<const Decl *, uint32_t> &FwdDecls,
    const LangOptions &LangOpts, bool SkipNextCompound = false);

[[nodiscard]] static ReflectionError GenerateStatement(
    ASTContext &ASTCtx, DiagnosticsEngine &Diags, const SourceManager &SM,
    ReflectionData &Refl, uint32_t AutoBindingSpace, uint32_t Depth,
    D3D12_HLSL_REFLECTION_FEATURE Features, uint32_t ParentNodeId,
    bool DefaultRowMaj, std::unordered_map<const Decl *, uint32_t> &FwdDecls,
    const LangOptions &LangOpts, D3D12_HLSL_NODE_TYPE Type,
    const VarDecl *VarDecl, Stmt *Body, Stmt *Init, Stmt *Self,
    bool IfAndHasElse = false) {

  uint32_t loc = uint32_t(Refl.Statements.size());

  const SourceRange &sourceRange = Self->getSourceRange();

  uint32_t nodeId;
  if (ReflectionError err =
          PushNextNodeId(nodeId, Refl, SM, LangOpts, "", nullptr, Type,
                         ParentNodeId, loc, &sourceRange, &FwdDecls))
    return err;

  Refl.Statements.push_back(ReflectionScopeStmt());

  if (VarDecl) {

    uint32_t typeId;
    if (ReflectionError err = GenerateTypeInfo(
            typeId, ASTCtx, Refl, VarDecl->getType(), DefaultRowMaj))
      return err;

    const SourceRange &sourceRange = VarDecl->getSourceRange();

    uint32_t nextNodeId;
    if (ReflectionError err =
            PushNextNodeId(nextNodeId, Refl, SM, LangOpts, VarDecl->getName(),
                           VarDecl, D3D12_HLSL_NODE_TYPE_VARIABLE, nodeId,
                           typeId, &sourceRange, &FwdDecls))
      return err;
  }

  uint32_t start = uint32_t(Refl.Nodes.size());

  if (ReflectionError err = RecursiveReflectBody(
          Init, ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1, Features,
          nodeId, DefaultRowMaj, FwdDecls, LangOpts, true))
    return err;

  if (ReflectionError err = ReflectionScopeStmt::Initialize(
          Refl.Statements[loc], nodeId, uint32_t(Refl.Nodes.size() - start),
          VarDecl, IfAndHasElse))
    return err;

  if (ReflectionError err = RecursiveReflectBody(
          Body, ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1, Features,
          nodeId, DefaultRowMaj, FwdDecls, LangOpts, true))
    return err;

  return ReflectionErrorSuccess;
}

static D3D12_HLSL_ENUM_TYPE GetEnumTypeFromQualType(ASTContext &ASTCtx,
                                                    QualType desugared) {

  const auto &semantics = ASTCtx.getTypeInfo(desugared);

  switch (semantics.Width) {

  default:
  case 32:
    return desugared->isUnsignedIntegerType() ? D3D12_HLSL_ENUM_TYPE_UINT
                                              : D3D12_HLSL_ENUM_TYPE_INT;

  case 16:
    return desugared->isUnsignedIntegerType() ? D3D12_HLSL_ENUM_TYPE_UINT16_T
                                              : D3D12_HLSL_ENUM_TYPE_INT16_T;

  case 64:
    return desugared->isUnsignedIntegerType() ? D3D12_HLSL_ENUM_TYPE_UINT64_T
                                              : D3D12_HLSL_ENUM_TYPE_INT64_T;
  }

  assert(false && "QualType of invalid type passed");
  return D3D12_HLSL_ENUM_TYPE_INT;
}

struct RecursiveStmtReflector : public StmtVisitor<RecursiveStmtReflector> {

  ASTContext &ASTCtx;
  DiagnosticsEngine &Diags;
  const SourceManager &SM;
  ReflectionData &Refl;
  uint32_t AutoBindingSpace;
  uint32_t Depth;
  D3D12_HLSL_REFLECTION_FEATURE Features;
  uint32_t ParentNodeId;
  bool DefaultRowMaj;
  std::unordered_map<const Decl *, uint32_t> &FwdDecls;
  const LangOptions &LangOpts;
  bool SkipNextCompound;

  ReflectionError LastError = ReflectionErrorSuccess;

  RecursiveStmtReflector(ASTContext &ASTCtx, DiagnosticsEngine &Diags,
                         const SourceManager &SM, ReflectionData &Refl,
                         uint32_t AutoBindingSpace, uint32_t Depth,
                         D3D12_HLSL_REFLECTION_FEATURE Features,
                         uint32_t ParentNodeId, bool DefaultRowMaj,
                         std::unordered_map<const Decl *, uint32_t> &FwdDecls,
                         const LangOptions &LangOpts, bool SkipNextCompound)
      : ASTCtx(ASTCtx), Diags(Diags), SM(SM), Refl(Refl),
        AutoBindingSpace(AutoBindingSpace), Depth(Depth), Features(Features),
        ParentNodeId(ParentNodeId), DefaultRowMaj(DefaultRowMaj),
        FwdDecls(FwdDecls), LangOpts(LangOpts),
        SkipNextCompound(SkipNextCompound) {}

  [[nodiscard]] ReflectionError TraverseStmt(Stmt *S) {

    if (LastError)
      return LastError;

    if (!S)
      return ReflectionErrorSuccess;

    while (AttributedStmt *AS = dyn_cast<AttributedStmt>(S))
      S = AS->getSubStmt();

    Visit(S);
    return LastError;
  }

  void VisitStmt(const Stmt *S) {}

  void VisitIfStmt(IfStmt *If) {

    if (LastError)
      return;
    
    uint32_t loc = uint32_t(Refl.IfSwitchStatements.size());

    const SourceRange &sourceRange = If->getSourceRange();

    uint32_t nodeId;
    if (ReflectionError err =
            PushNextNodeId(nodeId, Refl, SM, LangOpts, "", nullptr,
                           D3D12_HLSL_NODE_TYPE_IF_ROOT, ParentNodeId, loc,
                           &sourceRange, &FwdDecls)) {
      LastError = err;
      return;
    }

    Refl.IfSwitchStatements.push_back(ReflectionIfSwitchStmt());

    std::vector<Stmt*> branches;
    branches.reserve(2);
    branches.push_back(If);

    Stmt *child = If->getElse();

    while (child) {

      branches.push_back(child);

      if (IfStmt *ifChild = dyn_cast<IfStmt>(child))
        child = ifChild->getElse();

      else
        break;
    }

    bool hasElse = branches.size() > 1 && !isa<IfStmt>(branches.back());
    uint64_t counter = 0;

    for (Stmt *child : branches) {

      uint32_t loc = uint32_t(Refl.BranchStatements.size());
      Refl.BranchStatements.push_back(ReflectionBranchStmt());

      const SourceRange &sourceRange = child->getSourceRange();

      D3D12_HLSL_NODE_TYPE nodeType =
          !counter ? D3D12_HLSL_NODE_TYPE_IF_FIRST
                   : (hasElse && counter + 1 == branches.size()
                          ? D3D12_HLSL_NODE_TYPE_ELSE
                          : D3D12_HLSL_NODE_TYPE_ELSE_IF);
      ++counter;

      uint32_t childId;
      if (ReflectionError err =
              PushNextNodeId(childId, Refl, SM, LangOpts, "", nullptr, nodeType,
                             nodeId, loc, &sourceRange, &FwdDecls)) {
        LastError = err;
        return;
      }

      IfStmt *branch = dyn_cast_or_null<IfStmt>(child);

      VarDecl *cond = branch ? branch->getConditionVariable() : nullptr;

      if (cond) {

        uint32_t typeId;
        if (ReflectionError err = GenerateTypeInfo(
                typeId, ASTCtx, Refl, cond->getType(), DefaultRowMaj)) {
          LastError = err;
          return;
        }

        const SourceRange &sourceRange = cond->getSourceRange();

        uint32_t nextNodeId;
        if (ReflectionError err =
                PushNextNodeId(nextNodeId, Refl, SM, LangOpts, cond->getName(),
                               cond, D3D12_HLSL_NODE_TYPE_VARIABLE, childId,
                               typeId, &sourceRange, &FwdDecls)) {
          LastError = err;
          return;
        }
      }

      if (ReflectionError err = ReflectionBranchStmt::Initialize(
              Refl.BranchStatements[loc], childId, cond, true,
              D3D12_HLSL_ENUM_TYPE_UINT, uint64_t(-1))) {
        LastError = err;
        return;
      }

      uint32_t parentSelf = ParentNodeId;
      ParentNodeId = childId;

      Stmt *realChild = branch ? branch->getThen() : child;
      auto firstIt = realChild->child_begin();
      auto it = firstIt;

      if (it != realChild->child_end()) {

        ++it;

        if (it == realChild->child_end() && isa<CompoundStmt>(*firstIt)) {

          it = firstIt->child_begin();

          for (; it != firstIt->child_end(); ++it)
            if (ReflectionError err = TraverseStmt(*it)) {
              LastError = err;
              return;
            }
        }

        else {
          it = firstIt;
          for (; it != realChild->child_end(); ++it)
            if (ReflectionError err = TraverseStmt(*it)) {
              LastError = err;
              return;
            }
        }
      }

      ParentNodeId = parentSelf;
    }

    if (ReflectionError err = ReflectionIfSwitchStmt::Initialize(
            Refl.IfSwitchStatements[loc], nodeId, false, hasElse)) {
      LastError = err;
      return;
    }
  }

  void VisitForStmt(ForStmt *For) {

    if (LastError)
      return;

    LastError = GenerateStatement(
        ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1, Features,
        ParentNodeId, DefaultRowMaj, FwdDecls, LangOpts,
        D3D12_HLSL_NODE_TYPE_FOR, For->getConditionVariable(), For->getBody(),
        For->getInit(), For);
  }

  void VisitWhileStmt(WhileStmt *While) {

    if (LastError)
      return;

    LastError = GenerateStatement(
        ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1, Features,
        ParentNodeId, DefaultRowMaj, FwdDecls, LangOpts,
        D3D12_HLSL_NODE_TYPE_WHILE, While->getConditionVariable(),
        While->getBody(), nullptr, While);
  }

  void VisitDoStmt(DoStmt *Do) {

    if (LastError)
      return;

    const SourceRange &sourceRange = Do->getSourceRange();

    uint32_t scopeNode;
    if (ReflectionError err = PushNextNodeId(
            scopeNode, Refl, SM, LangOpts, "", nullptr, D3D12_HLSL_NODE_TYPE_DO,
            ParentNodeId, 0, &sourceRange, &FwdDecls)) {
      LastError = err;
      return;
    }

    LastError = RecursiveReflectBody(
        Do->getBody(), ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
        Features, scopeNode, DefaultRowMaj, FwdDecls, LangOpts, true);
  }

  void VisitSwitchStmt(SwitchStmt *Switch) {

    if (LastError)
      return;

    uint32_t loc = uint32_t(Refl.IfSwitchStatements.size());

    const SourceRange &sourceRange = Switch->getSourceRange();

    uint32_t nodeId;
    if (ReflectionError err =
            PushNextNodeId(nodeId, Refl, SM, LangOpts, "", nullptr,
                           D3D12_HLSL_NODE_TYPE_SWITCH, ParentNodeId, loc,
                           &sourceRange, &FwdDecls)) {
      LastError = err;
      return;
    }

    Refl.IfSwitchStatements.push_back(ReflectionIfSwitchStmt());

    VarDecl *cond = Switch->getConditionVariable();

    if (cond) {

      uint32_t typeId;
      if (ReflectionError err = GenerateTypeInfo(
              typeId, ASTCtx, Refl, cond->getType(), DefaultRowMaj)) {
        LastError = err;
        return;
      }

      const SourceRange &sourceRange = cond->getSourceRange();

      uint32_t nextNodeId;
      if (ReflectionError err =
              PushNextNodeId(nextNodeId, Refl, SM, LangOpts, cond->getName(),
                             cond, D3D12_HLSL_NODE_TYPE_VARIABLE, nodeId,
                             typeId, &sourceRange, &FwdDecls)) {
        LastError = err;
        return;
      }
    }

    Stmt *body = Switch->getBody();
    assert(body && "SwitchStmt has no body");

    bool hasDefault = false;

    for (Stmt *child : body->children()) {

      SwitchCase *switchCase = nullptr;
      uint64_t caseValue = uint64_t(-1);
      D3D12_HLSL_ENUM_TYPE valueType = D3D12_HLSL_ENUM_TYPE_INT;
      D3D12_HLSL_NODE_TYPE nodeType = D3D12_HLSL_NODE_TYPE_INVALID;

      bool isComplexCase = true;

      if (CaseStmt *caseStmt = dyn_cast<CaseStmt>(child)) {

        switchCase = caseStmt;

        llvm::APSInt result;

        if (caseStmt->getLHS()->isIntegerConstantExpr(result, ASTCtx)) {
          caseValue = result.getZExtValue();

          QualType desugared = caseStmt->getLHS()->getType();
          valueType = GetEnumTypeFromQualType(ASTCtx, desugared);
          isComplexCase = false;
        }

        else
          caseValue = uint64_t(-1);

        nodeType = D3D12_HLSL_NODE_TYPE_CASE;

      } else if (DefaultStmt *defaultStmt =
                     dyn_cast<DefaultStmt>(child)) {
        switchCase = defaultStmt;
        hasDefault = true;
        nodeType = D3D12_HLSL_NODE_TYPE_DEFAULT;
      }

      if (!switchCase)
        continue;

      uint32_t loc = uint32_t(Refl.BranchStatements.size());

      const SourceRange &sourceRange = switchCase->getSourceRange();

      uint32_t childId;
      if (ReflectionError err =
              PushNextNodeId(childId, Refl, SM, LangOpts, "", nullptr, nodeType,
                             nodeId, loc, &sourceRange, &FwdDecls)) {
        LastError = err;
        return;
      }

      Refl.BranchStatements.push_back(ReflectionBranchStmt());
      if (ReflectionError err = ReflectionBranchStmt::Initialize(
              Refl.BranchStatements.back(), childId, false, isComplexCase,
              valueType, caseValue)) {
        LastError = err;
        return;
      }

      uint32_t parentSelf = ParentNodeId;
      ParentNodeId = childId;

      auto realChild = switchCase->getSubStmt();

      auto firstIt = realChild->child_begin();
      auto it = firstIt;

      if (it != realChild->child_end()) {

        ++it;

        if (it == realChild->child_end() && isa<CompoundStmt>(*firstIt)) {
          for (Stmt *childChild : firstIt->children())
            if (ReflectionError err = TraverseStmt(childChild)) {
              LastError = err;
              return;
            }
        }

        else
          for (Stmt *childChild : realChild->children())
            if (ReflectionError err = TraverseStmt(childChild)) {
              LastError = err;
              return;
            }
      }

      ParentNodeId = parentSelf;
    }

    if (ReflectionError err = ReflectionIfSwitchStmt::Initialize(
            Refl.IfSwitchStatements[loc], nodeId, cond, hasDefault)) {
      LastError = err;
      return;
    }
  }

  void VisitCompoundStmt(CompoundStmt *C) {

    if (LastError)
      return;

    const SourceRange &sourceRange = C->getSourceRange();

    uint32_t scopeNode = ParentNodeId;

    if (!SkipNextCompound)
      if (ReflectionError err =
              PushNextNodeId(scopeNode, Refl, SM, LangOpts, "", nullptr,
                             D3D12_HLSL_NODE_TYPE_SCOPE, ParentNodeId, 0,
                             &sourceRange, &FwdDecls)) {
        LastError = err;
        return;
      }

    for (Stmt *child : C->body())
      if (ReflectionError err = RecursiveReflectBody(
              child, ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth + 1,
              Features, scopeNode, DefaultRowMaj, FwdDecls, LangOpts)) {
        LastError = err;
        return;
      }
  }

  void VisitDeclStmt(DeclStmt *DS) {

    if (LastError)
      return;

    for (Decl *D : DS->decls()) {
      if (VarDecl *varDecl = dyn_cast<VarDecl>(D)) {

        uint32_t typeId;
        if (ReflectionError err = GenerateTypeInfo(
                typeId, ASTCtx, Refl, varDecl->getType(), DefaultRowMaj)) {
          LastError = err;
          return;
        }

        const SourceRange &sourceRange = varDecl->getSourceRange();

        uint32_t nodeId;
        if (ReflectionError err =
                PushNextNodeId(nodeId, Refl, SM, LangOpts, varDecl->getName(),
                               varDecl, D3D12_HLSL_NODE_TYPE_VARIABLE,
                               ParentNodeId, typeId, &sourceRange, &FwdDecls)) {
          LastError = err;
          return;
        }
      }
    }
  }
};

[[nodiscard]] static ReflectionError
RecursiveReflectBody(Stmt *Statement, ASTContext &ASTCtx,
                     DiagnosticsEngine &Diags, const SourceManager &SM,
                     ReflectionData &Refl, uint32_t AutoBindingSpace,
                     uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
                     uint32_t ParentNodeId, bool DefaultRowMaj,
                     std::unordered_map<const Decl *, uint32_t> &FwdDecls,
                     const LangOptions &LangOpts, bool SkipNextCompound) {
  RecursiveStmtReflector Reflector(ASTCtx, Diags, SM, Refl, AutoBindingSpace,
                                   Depth, Features, ParentNodeId, DefaultRowMaj,
                                   FwdDecls, LangOpts, SkipNextCompound);
  return Reflector.TraverseStmt(Statement);
}

[[nodiscard]] static ReflectionError
RecursiveReflectHLSL(const DeclContext &Ctx, ASTContext &ASTCtx,
                     DiagnosticsEngine &Diags, const SourceManager &SM,
                     ReflectionData &Refl, uint32_t AutoBindingSpace,
                     uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
                     uint32_t ParentNodeId, bool DefaultRowMaj,
                     std::unordered_map<const Decl *, uint32_t> &FwdDecls);

class RecursiveReflector : public DeclVisitor<RecursiveReflector> {

  const DeclContext &Ctx;
  ASTContext &ASTCtx;
  const SourceManager &SM;
  DiagnosticsEngine &Diags;
  ReflectionData &Refl;
  uint32_t AutoBindingSpace;
  uint32_t Depth;
  D3D12_HLSL_REFLECTION_FEATURE Features;
  uint32_t ParentNodeId;
  bool DefaultRowMaj;
  std::unordered_map<const Decl *, uint32_t> &FwdDecls;

  ReflectionError LastError = ReflectionErrorSuccess;

  [[nodiscard]] ReflectionError PushVariable(ValueDecl *VD,
                                             D3D12_HLSL_NODE_TYPE NodeType) {

    uint32_t typeId;
    if (ReflectionError err = GenerateTypeInfo(typeId, ASTCtx, Refl,
                                               VD->getType(), DefaultRowMaj))
      return err;

    uint32_t nodeId;
    return PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(), VD->getName(),
                          VD, NodeType, ParentNodeId, typeId);
  }

public:
  RecursiveReflector(const DeclContext &Ctx, ASTContext &ASTCtx,
                     const SourceManager &SM, DiagnosticsEngine &Diags,
                     ReflectionData &Refl, uint32_t AutoBindingSpace,
                     uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
                     uint32_t ParentNodeId, bool DefaultRowMaj,
                     std::unordered_map<const Decl *, uint32_t> &FwdDecls)
      : Ctx(Ctx), ASTCtx(ASTCtx), SM(SM), Diags(Diags), Refl(Refl),
        AutoBindingSpace(AutoBindingSpace), Depth(Depth), Features(Features),
        ParentNodeId(ParentNodeId), DefaultRowMaj(DefaultRowMaj),
        FwdDecls(FwdDecls) {}

  [[nodiscard]] ReflectionError TraverseDeclContext() {

    if (LastError)
      return LastError;

    for (Decl *it : Ctx.decls()) {

      SourceLocation Loc = it->getLocation();
      if (Loc.isInvalid() ||
          SM.isInSystemHeader(Loc)) // TODO: We might want to include these for
                                    // a more complete picture.
        continue;

      Visit(it);

      if (LastError)
        return LastError;
    }

    return ReflectionErrorSuccess;
  }

  void VisitDecl(Decl *D) {}

  void VisitHLSLBufferDecl(HLSLBufferDecl *CB) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
      return;

    if (Depth != 0)
      return;

    uint32_t nodeId;

    if (ReflectionError err =
            PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(),
                           CB->getName(), CB, D3D12_HLSL_NODE_TYPE_REGISTER,
                           ParentNodeId, uint32_t(Refl.Registers.size()))) {
      LastError = err;
      return;
    }

    uint32_t bufferId;

    if (ReflectionError err =
            RegisterBuffer(bufferId, ASTCtx, Refl, SM, CB, nodeId,
                           D3D_CT_CBUFFER, DefaultRowMaj)) {
      LastError = err;
      return;
    }

    ReflectionShaderResource regD3D12;

    if (ReflectionError err = ReflectionShaderResource::Initialize(
            regD3D12, D3D_SIT_CBUFFER, 1, uint32_t(D3D_SIF_USERPACKED),
            D3D_RESOURCE_RETURN_TYPE(0), D3D_SRV_DIMENSION_UNKNOWN, nodeId,
            uint32_t(-1), bufferId)) {
      LastError = err;
      return;
    }

    Refl.Registers.push_back(regD3D12);
  }

  void VisitFunctionDecl(FunctionDecl *FD) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_FUNCTIONS))
      return;

    if (FD->isImplicit()) // Skip ctors, etc.
      return;

    const FunctionDecl *definition = nullptr;

    uint32_t nodeId;
    if (ReflectionError err = PushNextNodeId(
            nodeId, Refl, SM, ASTCtx.getLangOpts(), FD->getName(), FD,
            D3D12_HLSL_NODE_TYPE_FUNCTION, ParentNodeId,
            uint32_t(Refl.Functions.size()), nullptr, &FwdDecls)) {
      LastError = err;
      return;
    }

    if (nodeId == uint32_t(-1)) // Duplicate fwd definition
      return;

    bool hasDefinition = FD->hasBody(definition);
    ReflectionFunction func;

    if (ReflectionError err = ReflectionFunction::Initialize(
            func, nodeId, FD->getNumParams(),
            !FD->getReturnType().getTypePtr()->isVoidType(), hasDefinition)) {
      LastError = err;
      return;
    }

    for (uint32_t i = 0; i < func.GetNumParameters(); ++i)
      if (ReflectionError err = AddFunctionParameter(
              ASTCtx, FD->getParamDecl(i)->getType(), FD->getParamDecl(i), Refl,
              SM, nodeId, DefaultRowMaj)) {
        LastError = err;
        return;
      }

    if (func.HasReturn())
      if (ReflectionError err =
              AddFunctionParameter(ASTCtx, FD->getReturnType(), nullptr, Refl,
                                   SM, nodeId, DefaultRowMaj)) {
        LastError = err;
        return;
      }

    Refl.Functions.push_back(std::move(func));

    if (hasDefinition && (Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)) {

      Stmt *stmt = FD->getBody();

      for (Stmt *subStmt : stmt->children()) {

        if (!subStmt)
          continue;

        if (ReflectionError err = RecursiveReflectBody(
                subStmt, ASTCtx, Diags, SM, Refl, AutoBindingSpace, Depth,
                Features, nodeId, DefaultRowMaj, FwdDecls,
                ASTCtx.getLangOpts())) {
          LastError = err;
          return;
        }
      }
    }
  }

  void VisitEnumDecl(EnumDecl *ED) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
      return;

    uint32_t nodeId;

    if (ReflectionError err = PushNextNodeId(
            nodeId, Refl, SM, ASTCtx.getLangOpts(), ED->getName(), ED,
            D3D12_HLSL_NODE_TYPE_ENUM, ParentNodeId,
            uint32_t(Refl.Enums.size()), nullptr, &FwdDecls)) {
      LastError = err;
      return;
    }

    if (nodeId == uint32_t(-1)) // Duplicate fwd definition
      return;

    for (EnumConstantDecl *enumValue : ED->enumerators()) {

      uint32_t childNodeId;

      if (ReflectionError err = PushNextNodeId(
              childNodeId, Refl, SM, ASTCtx.getLangOpts(), enumValue->getName(),
              enumValue, D3D12_HLSL_NODE_TYPE_ENUM_VALUE, nodeId,
              uint32_t(Refl.EnumValues.size()))) {
        LastError = err;
        return;
      }

      Refl.EnumValues.push_back(
          {enumValue->getInitVal().getSExtValue(), childNodeId});
    }

    if (Refl.EnumValues.size() >= uint32_t(1 << 30)) {
      LastError = HLSL_REFL_ERR("Enum values overflow");
      return;
    }

    QualType enumType = ED->getIntegerType();
    QualType desugared = enumType.getDesugaredType(ASTCtx);

    D3D12_HLSL_ENUM_TYPE type = GetEnumTypeFromQualType(ASTCtx, desugared);

    Refl.Enums.push_back({nodeId, type});
  }

  void VisitTypedefDecl(TypedefDecl *TD) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
      return;

    uint32_t typeId;
    if (ReflectionError err = GenerateTypeInfo(
            typeId, ASTCtx, Refl, TD->getUnderlyingType(), DefaultRowMaj)) {
      LastError = err;
      return;
    }

    uint32_t nodeId;
    LastError =
        PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(), TD->getName(),
                       TD, D3D12_HLSL_NODE_TYPE_TYPEDEF, ParentNodeId, typeId);
  }

  void VisitTypeAliasDecl(TypeAliasDecl *TAD) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
      return;

    uint32_t typeId;
    if (ReflectionError err = GenerateTypeInfo(
            typeId, ASTCtx, Refl, TAD->getUnderlyingType(), DefaultRowMaj)) {
      LastError = err;
      return;
    }

    uint32_t nodeId;
    LastError =
        PushNextNodeId(nodeId, Refl, SM, ASTCtx.getLangOpts(), TAD->getName(),
                       TAD, D3D12_HLSL_NODE_TYPE_USING, ParentNodeId, typeId);
  }

  void VisitFieldDecl(FieldDecl *FD) {

    if (LastError)
      return;

    LastError = PushVariable(FD, D3D12_HLSL_NODE_TYPE_VARIABLE);
  }

  void VisitValueDecl(ValueDecl *VD) {

    if (LastError)
      return;

    if (isa<ParmVarDecl>(VD)) // Skip parameters, already handled explicitly
      return;

    VarDecl *varDecl = dyn_cast<VarDecl>(VD);

    if (varDecl && varDecl->hasAttr<HLSLGroupSharedAttr>()) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        return;

      LastError = PushVariable(VD, D3D12_HLSL_NODE_TYPE_GROUPSHARED_VARIABLE);
      return;
    }

    if (varDecl && varDecl->getStorageClass() == StorageClass::SC_Static) {

      if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
        return;

      LastError = PushVariable(VD, D3D12_HLSL_NODE_TYPE_STATIC_VARIABLE);
      return;
    }

    uint32_t arraySize = 1;
    QualType type = VD->getType();
    std::vector<uint32_t> arrayElem;

    while (const ConstantArrayType *arr = dyn_cast<ConstantArrayType>(type)) {
      uint32_t current = arr->getSize().getZExtValue();
      arrayElem.push_back(current);
      arraySize *= arr->getSize().getZExtValue();
      type = arr->getElementType();
    }

    if (!IsHLSLResourceType(type)) {

      // Handle $Globals or regular variables

      if (varDecl &&
          (Depth == 0 || Features & D3D12_HLSL_REFLECTION_FEATURE_SCOPES)) {

        LastError = PushVariable(VD, D3D12_HLSL_NODE_TYPE_VARIABLE);
      }

      return;
    }

    if (Depth != 0 || !(Features & D3D12_HLSL_REFLECTION_FEATURE_BASICS))
      return;

    LastError = FillReflectionRegisterAt(
        Ctx, ASTCtx, SM, Diags, type, arraySize, VD, arrayElem, Refl,
        AutoBindingSpace, ParentNodeId, DefaultRowMaj);
  }

  void VisitRecordDecl(RecordDecl *RD) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
      return;

    bool isDefinition = RD->isThisDeclarationADefinition();

    D3D12_HLSL_NODE_TYPE type = D3D12_HLSL_NODE_TYPE_RESERVED;

    switch (RD->getTagKind()) {

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
        if (ReflectionError err = GenerateTypeInfo(
                typeId, ASTCtx, Refl, RD->getASTContext().getRecordType(RD),
                DefaultRowMaj)) {
          LastError = err;
          return;
        }

      uint32_t self;
      if (ReflectionError err = PushNextNodeId(
              self, Refl, SM, ASTCtx.getLangOpts(), RD->getName(), RD, type,
              ParentNodeId, typeId, nullptr, &FwdDecls)) {
        LastError = err;
        return;
      }

      if (self == uint32_t(-1)) // Duplicate fwd definition
        return;

      if (isDefinition)
        LastError = RecursiveReflectHLSL(*RD, ASTCtx, Diags, SM, Refl,
                                         AutoBindingSpace, Depth + 1, Features,
                                         self, DefaultRowMaj, FwdDecls);
    }
  }

  void VisitNamespaceDecl(NamespaceDecl *ND) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_NAMESPACES))
      return;

    uint32_t nodeId;
    if (ReflectionError err = PushNextNodeId(
            nodeId, Refl, SM, ASTCtx.getLangOpts(), ND->getName(), ND,
            D3D12_HLSL_NODE_TYPE_NAMESPACE, ParentNodeId, 0)) {
      LastError = err;
      return;
    }

    LastError = RecursiveReflectHLSL(*ND, ASTCtx, Diags, SM, Refl,
                                     AutoBindingSpace, Depth + 1, Features,
                                     nodeId, DefaultRowMaj, FwdDecls);
  }

  /*void VisitTypeAliasDecl(TypeAliasDecl *TAD) {

    if (LastError)
      return;

    if (!(Features & D3D12_HLSL_REFLECTION_FEATURE_USER_TYPES))
      return;

      // TODO: Implement. TAD->print(pfStream, printingPolicy);
  }*/
};

[[nodiscard]] static ReflectionError
RecursiveReflectHLSL(const DeclContext &Ctx, ASTContext &ASTCtx,
                     DiagnosticsEngine &Diags, const SourceManager &SM,
                     ReflectionData &Refl, uint32_t AutoBindingSpace,
                     uint32_t Depth, D3D12_HLSL_REFLECTION_FEATURE Features,
                     uint32_t ParentNodeId, bool DefaultRowMaj,
                     std::unordered_map<const Decl *, uint32_t> &FwdDecls) {

  RecursiveReflector Reflector(Ctx, ASTCtx, SM, Diags, Refl, AutoBindingSpace,
                               Depth, Features, ParentNodeId, DefaultRowMaj,
                               FwdDecls);
  return Reflector.TraverseDeclContext();
}

[[nodiscard]] ReflectionError
HLSLReflectionDataFromAST(ReflectionData &Result, CompilerInstance &Compiler,
                          TranslationUnitDecl &Ctx, uint32_t AutoBindingSpace,
                          D3D12_HLSL_REFLECTION_FEATURE Features,
                          bool DefaultRowMaj) {

  DiagnosticsEngine &Diags = Ctx.getParentASTContext().getDiagnostics();
  const SourceManager &SM = Compiler.getSourceManager();

  Result = {};
  Result.Features = Features;

  if (Features & D3D12_HLSL_REFLECTION_FEATURE_SYMBOL_INFO) {
    Result.Strings.push_back("");
    Result.StringsToId[""] = 0;
    Result.NodeSymbols.push_back({});

    if (ReflectionError err = ReflectionNodeSymbol::Initialize(
            Result.NodeSymbols[0], 0, uint16_t(-1), 0, 0, 0, 0)) {
      llvm::errs() << "HLSLReflectionDataFromAST: Failed to add root symbol: "
                   << err;
      Result = {};
      return err;
    }
  }

  Result.Nodes.push_back({});
  if (ReflectionError err = ReflectionNode::Initialize(
          Result.Nodes[0], D3D12_HLSL_NODE_TYPE_NAMESPACE, false, 0, 0, 0,
          0xFFFF, 0, uint16_t(-1), D3D_INTERPOLATION_UNDEFINED)) {
    llvm::errs() << "HLSLReflectionDataFromAST: Failed to add root node: "
                 << err;
    Result = {};
    return err;
  }

  std::unordered_map<const Decl *, uint32_t> fwdDecls;

  if (ReflectionError err = RecursiveReflectHLSL(
          Ctx, Compiler.getASTContext(), Diags, SM, Result, AutoBindingSpace, 0,
          Features, 0, DefaultRowMaj, fwdDecls)) {
    llvm::errs() << "HLSLReflectionDataFromAST: Failed: " << err;
    Result = {};
    return err;
  }

  return ReflectionErrorSuccess;
}

} // namespace hlsl
