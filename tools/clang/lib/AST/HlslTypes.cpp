//===--- HlslTypes.cpp  - Type system for HLSL                 ----*- C++
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HlslTypes.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines the HLSL type system interface.                           //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "clang/AST/CanonicalType.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Type.h"
#include "clang/Sema/AttributeList.h" // conceptually ParsedAttributes
#include "clang/AST/ASTContext.h"

using namespace clang;

namespace hlsl {

/// <summary>Try to convert HLSL template vector/matrix type to
/// ExtVectorType.</summary>
const clang::ExtVectorType *
ConvertHLSLVecMatTypeToExtVectorType(const clang::ASTContext &context,
                                     clang::QualType type) {
  const Type *Ty = type.getCanonicalType().getTypePtr();

  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      // TODO: check pointer instead of name
      if (templateDecl->getName() == "vector") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg0 = argList[0];
        const TemplateArgument &arg1 = argList[1];
        QualType elemTy = arg0.getAsType();
        llvm::APSInt elmSize = arg1.getAsIntegral();
        return context.getExtVectorType(elemTy, elmSize.getLimitedValue())
            ->getAs<ExtVectorType>();
      }
    }
  }
  return nullptr;
}

bool IsHLSLVecMatType(clang::QualType type) {
  const Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "vector") {
        return true;
      } else if (templateDecl->getName() == "matrix") {
        return true;
      }
    }
  }
  return false;
}

bool IsHLSLMatType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "matrix") {
        return true;
      }
    }
  }
  return false;
}

bool IsHLSLVecType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "vector") {
        return true;
      }
    }
  }
  return false;
}

bool IsHLSLNumericOrAggregateOfNumericType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (isa<RecordType>(Ty)) {
    if (IsHLSLVecMatType(type))
      return true;
    return IsHLSLNumericUserDefinedType(type);
  } else if (type->isArrayType()) {
    return IsHLSLNumericOrAggregateOfNumericType(QualType(type->getArrayElementTypeNoTypeQual(), 0));
  }

  // Chars can only appear as part of strings, which we don't consider numeric.
  const BuiltinType* BuiltinTy = dyn_cast<BuiltinType>(Ty);
  return BuiltinTy != nullptr && BuiltinTy->getKind() != BuiltinType::Kind::Char_S;
}

bool IsHLSLNumericUserDefinedType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    const RecordDecl *RD = RT->getDecl();
    if (isa<ClassTemplateSpecializationDecl>(RD)) {
      return false;   // UDT are not templates
    }
    // TODO: avoid check by name
    StringRef name = RD->getName();
    if (name == "ByteAddressBuffer" ||
        name == "RWByteAddressBuffer" ||
        name == "RaytracingAccelerationStructure")
      return false;
    for (auto member : RD->fields()) {
      if (!IsHLSLNumericOrAggregateOfNumericType(member->getType()))
        return false;
    }
    return true;
  }
  return false;
}

// Aggregate types are arrays and user-defined structs
bool IsHLSLAggregateType(clang::QualType type) {
  type = type.getCanonicalType();
  if (isa<clang::ArrayType>(type)) return true;

  const RecordType *Record = dyn_cast<RecordType>(type);
  return Record != nullptr
    && !IsHLSLVecMatType(type) && !IsHLSLResourceType(type)
    && !dyn_cast<ClassTemplateSpecializationDecl>(Record->getAsCXXRecordDecl());
}

clang::QualType GetElementTypeOrType(clang::QualType type) {
  if (const RecordType *RT = type->getAs<RecordType>()) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      // TODO: check pointer instead of name
      if (templateDecl->getName() == "vector") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        return argList[0].getAsType();
      }
      else if (templateDecl->getName() == "matrix") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        return argList[0].getAsType();
      }
    }
  }
  return type;
}

bool HasHLSLMatOrientation(clang::QualType type, bool *pIsRowMajor) {
  const AttributedType *AT = type->getAs<AttributedType>();
  while (AT) {
    AttributedType::Kind kind = AT->getAttrKind();
    switch (kind) {
    case AttributedType::attr_hlsl_row_major:
      if (pIsRowMajor) *pIsRowMajor = true;
      return true;
    case AttributedType::attr_hlsl_column_major:
      if (pIsRowMajor) *pIsRowMajor = false;
      return true;
    }
    AT = AT->getLocallyUnqualifiedSingleStepDesugaredType()->getAs<AttributedType>();
  }
  return false;
}

bool IsHLSLMatRowMajor(clang::QualType type, bool defaultValue) {
  bool result = defaultValue;
  HasHLSLMatOrientation(type, &result);
  return result;
}

bool IsHLSLUnsigned(clang::QualType type) {
  if (type->getAs<clang::BuiltinType>() == nullptr) {
    type = type.getCanonicalType().getNonReferenceType();

    if (IsHLSLVecMatType(type))
      type = GetElementTypeOrType(type);

    if (type->isExtVectorType())
      type = type->getAs<clang::ExtVectorType>()->getElementType();
  }

  return type->isUnsignedIntegerType();
}

bool HasHLSLUNormSNorm(clang::QualType type, bool *pIsSNorm) {
  // snorm/unorm can be on outer vector/matrix as well as element type
  // in the template form.  Outer-most type attribute wins.
  // The following drills into attributed type for outer type,
  // setting *pIsSNorm and returning true if snorm/unorm found.
  // If not found on outer type, fall back to element type if different,
  // indicating a vector or matrix, and try again.
  clang::QualType elementType = GetElementTypeOrType(type);
  while (true) {
    const AttributedType *AT = type->getAs<AttributedType>();
    while (AT) {
      AttributedType::Kind kind = AT->getAttrKind();
      switch (kind) {
      case AttributedType::attr_hlsl_snorm:
        if (pIsSNorm) *pIsSNorm = true;
        return true;
      case AttributedType::attr_hlsl_unorm:
        if (pIsSNorm) *pIsSNorm = false;
        return true;
      }
      AT = AT->getLocallyUnqualifiedSingleStepDesugaredType()->getAs<AttributedType>();
    }
    if (type == elementType)
      break;
    type = elementType;
  }
  return false;
}

bool HasHLSLGloballyCoherent(clang::QualType type) {
  const AttributedType *AT = type->getAs<AttributedType>();
  while (AT) {
    AttributedType::Kind kind = AT->getAttrKind();
    switch (kind) {
    case AttributedType::attr_hlsl_globallycoherent:
      return true;
    }
    AT = AT->getLocallyUnqualifiedSingleStepDesugaredType()
             ->getAs<AttributedType>();
  }
  return false;
}

/// Checks whether the pAttributes indicate a parameter is inout or out; if
/// inout, pIsIn will be set to true.
bool IsParamAttributedAsOut(_In_opt_ clang::AttributeList *pAttributes,
  _Out_opt_ bool *pIsIn);

/// <summary>Gets the type with structural information (elements and shape) for
/// the given type.</summary>
/// <remarks>This function will strip lvalue/rvalue references, attributes and
/// qualifiers.</remarks>
QualType GetStructuralForm(QualType type) {
  if (type.isNull()) {
    return type;
  }

  const ReferenceType *RefType = nullptr;
  const AttributedType *AttrType = nullptr;
  while ((RefType = dyn_cast<ReferenceType>(type)) ||
         (AttrType = dyn_cast<AttributedType>(type))) {
    type = RefType ? RefType->getPointeeType() : AttrType->getEquivalentType();
  }

  return type->getCanonicalTypeUnqualified();
}

uint32_t GetElementCount(clang::QualType type) {
  uint32_t rowCount, colCount;
  GetRowsAndColsForAny(type, rowCount, colCount);
  return rowCount * colCount;
}

/// <summary>Returns the number of elements in the specified array
/// type.</summary>
uint32_t GetArraySize(clang::QualType type) {
  assert(type->isArrayType() && "otherwise caller shouldn't be invoking this");

  if (type->isConstantArrayType()) {
    const ConstantArrayType *arrayType =
        (const ConstantArrayType *)type->getAsArrayTypeUnsafe();
    return arrayType->getSize().getLimitedValue();
  } else {
    return 0;
  }
}

/// <summary>Returns the number of elements in the specified vector
/// type.</summary>
uint32_t GetHLSLVecSize(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "vector" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg1 = argList[1];
  llvm::APSInt vecSize = arg1.getAsIntegral();
  return vecSize.getLimitedValue();
}

void GetRowsAndCols(clang::QualType type, uint32_t &rowCount,
                    uint32_t &colCount) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "matrix" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg1 = argList[1];
  const TemplateArgument &arg2 = argList[2];
  llvm::APSInt rowSize = arg1.getAsIntegral();
  llvm::APSInt colSize = arg2.getAsIntegral();
  rowCount = rowSize.getLimitedValue();
  colCount = colSize.getLimitedValue();
}

bool IsArrayConstantStringType(const QualType type) {
  DXASSERT_NOMSG(type->isArrayType());
  return type->getArrayElementTypeNoTypeQual()->isSpecificBuiltinType(BuiltinType::Char_S);
}

bool IsPointerStringType(const QualType type) {
  DXASSERT_NOMSG(type->isPointerType());
  return type->getPointeeType()->isSpecificBuiltinType(BuiltinType::Char_S);
}

bool IsStringType(const QualType type) {
  QualType canType = type.getCanonicalType();
  return canType->isPointerType() && IsPointerStringType(canType);
}

bool IsStringLiteralType(const QualType type) {
  QualType canType = type.getCanonicalType();
  return canType->isArrayType() && IsArrayConstantStringType(canType);
}

void GetRowsAndColsForAny(QualType type, uint32_t &rowCount,
                          uint32_t &colCount) {
  assert(!type.isNull());

  type = GetStructuralForm(type);
  rowCount = 1;
  colCount = 1;
  const Type *Ty = type.getCanonicalType().getTypePtr();
  if (type->isArrayType() && !IsArrayConstantStringType(type)) {
    if (type->isConstantArrayType()) {
      const ConstantArrayType *arrayType =
          (const ConstantArrayType *)type->getAsArrayTypeUnsafe();
      colCount = arrayType->getSize().getLimitedValue();
    } else {
      colCount = 0;
    }
  } else if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "matrix") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg1 = argList[1];
        const TemplateArgument &arg2 = argList[2];
        llvm::APSInt rowSize = arg1.getAsIntegral();
        llvm::APSInt colSize = arg2.getAsIntegral();
        rowCount = rowSize.getLimitedValue();
        colCount = colSize.getLimitedValue();
      } else if (templateDecl->getName() == "vector") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg1 = argList[1];
        llvm::APSInt rowSize = arg1.getAsIntegral();
        colCount = rowSize.getLimitedValue();
      }
    }
  }
}

void GetHLSLMatRowColCount(clang::QualType type, unsigned int &row,
                           unsigned int &col) {
  GetRowsAndColsForAny(type, row, col);
}
clang::QualType GetHLSLVecElementType(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "vector" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  QualType elemTy = arg0.getAsType();
  return elemTy;
}
clang::QualType GetHLSLMatElementType(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "matrix" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  QualType elemTy = arg0.getAsType();
  return elemTy;
}
// TODO: Add type cache to ASTContext.
bool IsHLSLInputPatchType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "InputPatch") {
        return true;
      }
    }
  }
  return false;
}
bool IsHLSLOutputPatchType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "OutputPatch") {
        return true;
      }
    }
  }
  return false;
}
bool IsHLSLPointStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "PointStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLLineStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "LineStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLTriangleStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "TriangleStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLStreamOutputType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "PointStream")
        return true;
      if (templateDecl->getName() == "LineStream")
        return true;
      if (templateDecl->getName() == "TriangleStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLResourceType(clang::QualType type) {
  if (const RecordType *RT = type->getAs<RecordType>()) {
    StringRef name = RT->getDecl()->getName();
    if (name == "Texture1D" || name == "RWTexture1D")
      return true;
    if (name == "Texture2D" || name == "RWTexture2D")
      return true;
    if (name == "Texture2DMS" || name == "RWTexture2DMS")
      return true;
    if (name == "Texture3D" || name == "RWTexture3D")
      return true;
    if (name == "TextureCube" || name == "RWTextureCube")
      return true;

    if (name == "Texture1DArray" || name == "RWTexture1DArray")
      return true;
    if (name == "Texture2DArray" || name == "RWTexture2DArray")
      return true;
    if (name == "Texture2DMSArray" || name == "RWTexture2DMSArray")
      return true;
    if (name == "TextureCubeArray" || name == "RWTextureCubeArray")
      return true;

    if (name == "FeedbackTexture2D" || name == "FeedbackTexture2DArray")
      return true;

    if (name == "ByteAddressBuffer" || name == "RWByteAddressBuffer")
      return true;

    if (name == "StructuredBuffer" || name == "RWStructuredBuffer")
      return true;

    if (name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer")
      return true;

    if (name == "Buffer" || name == "RWBuffer")
      return true;

    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;

    if (name == "ConstantBuffer")
      return true;

    if (name == "RaytracingAccelerationStructure")
      return true;
  }
  return false;
}

bool IsHLSLSubobjectType(clang::QualType type) {
  DXIL::SubobjectKind kind;
  DXIL::HitGroupType hgType;
  return GetHLSLSubobjectKind(type, kind, hgType);
}

bool GetHLSLSubobjectKind(clang::QualType type, DXIL::SubobjectKind &subobjectKind, DXIL::HitGroupType &hgType) {
  hgType = (DXIL::HitGroupType)(-1);
  type = type.getCanonicalType();
  if (const RecordType *RT = type->getAs<RecordType>()) {
    StringRef name = RT->getDecl()->getName();
    switch (name.size()) {
    case 17:
      return name == "StateObjectConfig" ? (subobjectKind = DXIL::SubobjectKind::StateObjectConfig, true) : false;
    case 18:
      return name == "LocalRootSignature" ? (subobjectKind = DXIL::SubobjectKind::LocalRootSignature, true) : false;
    case 19:
      return name == "GlobalRootSignature" ? (subobjectKind = DXIL::SubobjectKind::GlobalRootSignature, true) : false;
    case 29:
      return name == "SubobjectToExportsAssociation" ? (subobjectKind = DXIL::SubobjectKind::SubobjectToExportsAssociation, true) : false;
    case 22:
      return name == "RaytracingShaderConfig" ? (subobjectKind = DXIL::SubobjectKind::RaytracingShaderConfig, true) : false;
    case 24:
      return name == "RaytracingPipelineConfig" ? (subobjectKind = DXIL::SubobjectKind::RaytracingPipelineConfig, true) : false;
    case 25:
      return name == "RaytracingPipelineConfig1" ? (subobjectKind = DXIL::SubobjectKind::RaytracingPipelineConfig1, true) : false;
    case 16:
      if (name == "TriangleHitGroup") {
        subobjectKind = DXIL::SubobjectKind::HitGroup;
        hgType = DXIL::HitGroupType::Triangle;
        return true;
      }
      return false;
    case 27:
      if (name == "ProceduralPrimitiveHitGroup") {
        subobjectKind = DXIL::SubobjectKind::HitGroup;
        hgType = DXIL::HitGroupType::ProceduralPrimitive;
        return true;
      }
      return false;
    }
  }
  return false;
}

bool IsHLSLRayQueryType(clang::QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      StringRef name = templateDecl->getName();
      if (name == "RayQuery")
        return true;
    }
  }
  return false;
}

QualType GetHLSLResourceResultType(QualType type) {
  // Don't canonicalize the type as to not lose snorm in Buffer<snorm float>
  const RecordType *RT = type->getAs<RecordType>();
  const RecordDecl* RD = RT->getDecl();

  if (const ClassTemplateSpecializationDecl *templateDecl =
    dyn_cast<ClassTemplateSpecializationDecl>(RD)) {
    
    if (RD->getName().startswith("FeedbackTexture")) {
      // Feedback textures are write-only and the data is opaque,
      // so there is no result type per se.
      return {};
    }
    
    // Type-templated resource types

    // Prefer getting the template argument from the TemplateSpecializationType sugar,
    // since this preserves 'snorm' from 'Buffer<snorm float>' which is lost on the
    // ClassTemplateSpecializationDecl since it's considered type sugar.
    const TemplateArgument* templateArg = &templateDecl->getTemplateArgs()[0];
    if (const TemplateSpecializationType *specializationType = type->getAs<TemplateSpecializationType>()) {
      if (specializationType->getNumArgs() >= 1) {
        templateArg = &specializationType->getArg(0);
      }
    }

    if (templateArg->getKind() == TemplateArgument::ArgKind::Type)
      return templateArg->getAsType();
  }

  // Non-type-templated resource types like [RW][RasterOrder]ByteAddressBuffer
  // Get the result type from handle field.
  FieldDecl* HandleFieldDecl = *(RD->field_begin());
  DXASSERT(HandleFieldDecl->getName() == "h", "Resource must have a handle field");
  return HandleFieldDecl->getType();
}

unsigned GetHLSLResourceTemplateUInt(clang::QualType type) {
  const ClassTemplateSpecializationDecl* templateDecl = cast<ClassTemplateSpecializationDecl>(
    type->castAs<RecordType>()->getDecl());
  return (unsigned)templateDecl->getTemplateArgs()[0].getAsIntegral().getZExtValue();
}

bool IsIncompleteHLSLResourceArrayType(clang::ASTContext &context,
                                       clang::QualType type) {
  if (type->isIncompleteArrayType()) {
    const IncompleteArrayType *IAT = context.getAsIncompleteArrayType(type);
    QualType EltTy = IAT->getElementType();
    if (IsHLSLResourceType(EltTy))
      return true;
  }
  return false;
}
QualType GetHLSLInputPatchElementType(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[0].getAsType();
}
unsigned GetHLSLInputPatchCount(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[1].getAsIntegral().getLimitedValue();
}
clang::QualType GetHLSLOutputPatchElementType(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[0].getAsType();
}
unsigned GetHLSLOutputPatchCount(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[1].getAsIntegral().getLimitedValue();
}

_Use_decl_annotations_
bool IsParamAttributedAsOut(clang::AttributeList *pAttributes, bool *pIsIn) {
  bool anyFound = false;
  bool inFound = false;
  bool outFound = false;
  while (pAttributes != nullptr) {
    switch (pAttributes->getKind()) {
    case AttributeList::AT_HLSLIn:
      anyFound = true;
      inFound = true;
      break;
    case AttributeList::AT_HLSLOut:
      anyFound = true;
      outFound = true;
      break;
    case AttributeList::AT_HLSLInOut:
      anyFound = true;
      outFound = true;
      inFound = true;
      break;
    default:
      // Ignore the majority of attributes that don't have in/out characteristics
      break;
    }
    pAttributes = pAttributes->getNext();
  }
  if (pIsIn)
    *pIsIn = inFound || anyFound == false;
  return outFound;
}

_Use_decl_annotations_
hlsl::ParameterModifier ParamModFromAttributeList(clang::AttributeList *pAttributes) {
  bool isIn, isOut;
  isOut = IsParamAttributedAsOut(pAttributes, &isIn);
  return ParameterModifier::FromInOut(isIn, isOut);
}

hlsl::ParameterModifier ParamModFromAttrs(llvm::ArrayRef<InheritableAttr *> attributes) {
  bool isIn = false, isOut = false;
  for (InheritableAttr * attr : attributes) {
    if (isa<HLSLInAttr>(attr))
      isIn = true;
    else if (isa<HLSLOutAttr>(attr))
      isOut = true;
    else if (isa<HLSLInOutAttr>(attr))
      isIn = isOut = true;
  }
  // Without any specifications, default to in.
  if (!isIn && !isOut) {
    isIn = true;
  }
  return ParameterModifier::FromInOut(isIn, isOut);
}

HLSLScalarType MakeUnsigned(HLSLScalarType T) {
    switch (T) {
    case HLSLScalarType_int:
        return HLSLScalarType_uint;
    case HLSLScalarType_int_min16:
        return HLSLScalarType_uint_min16;
    case HLSLScalarType_int64:
        return HLSLScalarType_uint64;
    case HLSLScalarType_int16:
        return HLSLScalarType_uint16;
    default:
        // Only signed int types are relevant.
        break;
    }
    return T;
}

}
