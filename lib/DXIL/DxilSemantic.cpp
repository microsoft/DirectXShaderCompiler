///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSemantic.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilSigPoint.h"
#include "dxc/DXIL/DxilSemantic.h"
#include "dxc/DXIL/DxilSignature.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/Global.h"

#include <string>

using std::string;

namespace hlsl {

//------------------------------------------------------------------------------
//
// Semantic class methods.
//
Semantic::Semantic(Kind Kind,
                   const char *pszName,
                   CompTy allowedTys,
                   SizeClass minCompCount,
                   SizeClass maxCompCount)
: m_Kind(Kind)
, m_pszName(pszName)
, m_allowedTys(allowedTys)
, m_minCompCount(minCompCount)
, m_maxCompCount(maxCompCount)
{
}

const Semantic *Semantic::GetByName(llvm::StringRef name) {
  if (!HasSVPrefix(name))
    return GetArbitrary();

  // The search is a simple linear scan as it is fairly infrequent operation and the list is short.
  // The search can be improved if linear traversal has inadequate performance.
  for (unsigned i = (unsigned)Kind::Arbitrary + 1; i < (unsigned)Kind::Invalid; i++) {
    if (name.compare_lower(ms_SemanticTable[i].m_pszName) == 0)
      return &ms_SemanticTable[i];
  }

  return GetInvalid();
}

const Semantic *Semantic::GetByName(llvm::StringRef Name, DXIL::SigPointKind sigPointKind,
                                    unsigned MajorVersion, unsigned MinorVersion) {
  return Get(GetByName(Name)->GetKind(), sigPointKind, MajorVersion, MinorVersion);
}

const Semantic *Semantic::Get(Kind kind) {
  if (kind < Kind::Invalid)
    return &Semantic::ms_SemanticTable[(unsigned)kind];
  return GetInvalid();
}

const Semantic *Semantic::Get(Kind kind, DXIL::SigPointKind sigPointKind,
                              unsigned MajorVersion, unsigned MinorVersion) {
  if (sigPointKind == DXIL::SigPointKind::Invalid)
    return GetInvalid();
  const Semantic* pSemantic = Get(kind);
  DXIL::SemanticInterpretationKind SI = SigPoint::GetInterpretation(pSemantic->GetKind(), sigPointKind, MajorVersion, MinorVersion);
  if(SI == DXIL::SemanticInterpretationKind::NA)
    return GetInvalid();
  if(SI == DXIL::SemanticInterpretationKind::Arb)
    return GetArbitrary();
  return pSemantic;
}

const Semantic *Semantic::GetInvalid() {
  return &Semantic::ms_SemanticTable[(unsigned)Kind::Invalid];
}
const Semantic *Semantic::GetArbitrary() {
  return &Semantic::ms_SemanticTable[(unsigned)Kind::Arbitrary];
}

bool Semantic::HasSVPrefix(llvm::StringRef Name) {
  return Name.size() >= 3 && (Name[0] == 'S' || Name[0] == 's') &&
      (Name[1] == 'V' || Name[1] == 'v') && Name[2] == '_';
}

void Semantic::DecomposeNameAndIndex(llvm::StringRef FullName, llvm::StringRef *pName, unsigned *pIndex) {
  unsigned L = FullName.size(), i;

  for (i = L; i > 0; i--) {
    char d = FullName[i - 1];
    if ('0' > d || d > '9')
      break;
  }

  *pName = FullName.substr(0, i);

  if (i < L)
    *pIndex = atoi(FullName.data() + i);
  else
    *pIndex = 0;
}

Semantic::Kind Semantic::GetKind() const {
  return m_Kind;
}

const char *Semantic::GetName() const {
  return m_pszName;
}

bool Semantic::IsArbitrary() const {
  return GetKind() == Kind::Arbitrary;
}

bool Semantic::IsInvalid() const {
  return m_Kind == Kind::Invalid;
}

Semantic::SizeClass Semantic::GetCompCount(llvm::Type* ty) const {

  if (!ty->isVectorTy() && !dxilutil::IsIntegerOrFloatingPointType(ty))
    return SizeClass::Unknown;

  if (ty->isVectorTy()) {
    if (ty->getVectorNumElements() == 1) {
      return SizeClass::Vec1;
    }
    else if (ty->getVectorNumElements() == 2) {
      return SizeClass::Vec2;
    }
    else if (ty->getVectorNumElements() == 3) {
      return SizeClass::Vec3;
    }
    else if (ty->getVectorNumElements() == 4) {
      return SizeClass::Vec4;
    }
    else {
      DXASSERT(false, "Unexpected number of vector elements.");
      return SizeClass::Unknown;
    }
  }

  return SizeClass::Scalar;
}

Semantic::CompTy Semantic::GetCompType(llvm::Type* ty) const {

  if (!ty->isVectorTy() && !dxilutil::IsIntegerOrFloatingPointType(ty))
    return CompTy::AnyTy;

  if (ty->isVectorTy())
    ty = ty->getScalarType();

  // must be an integer or a floating point type here
  DXASSERT_NOMSG(dxilutil::IsIntegerOrFloatingPointType(ty));
  if (ty->getScalarType()->isIntegerTy()) {
    if (ty->getScalarSizeInBits() == 1) {
      return CompTy::BoolTy;
    } else if (ty->getScalarSizeInBits() == 16) {
      return CompTy::Int16Ty;
    } else if (ty->getScalarSizeInBits() == 32) {
      return CompTy::Int32Ty;
    } else {
      return CompTy::Int64Ty;
    }
  } else {
    if (ty->isHalfTy()) {
      return CompTy::HalfTy;
    } else if (ty->isFloatTy()) {
      return CompTy::FloatTy;
    } else {
      DXASSERT_NOMSG(ty->isDoubleTy());
      return CompTy::DoubleTy;
    }
  }
}

static bool IsScalarOrVectorTy(llvm::Type* ty) {
  if (dxilutil::IsIntegerOrFloatingPointType(ty))
    return true;
  if (ty->isVectorTy() &&
    dxilutil::IsIntegerOrFloatingPointType(ty->getVectorElementType()))
    return true;
  return false;
}

bool Semantic::IsSupportedType(llvm::Type* semTy) const {

  if (m_Kind == Kind::Invalid)
    return false;

  // Skip type checking for Arbitrary kind
  if (m_Kind == Kind::Arbitrary)
    return true;

  if (!IsScalarOrVectorTy(semTy)) {
    // We only allow scalar or vector types as a valid semantic type except in some cases
    // such as Clip/Cull or Tessfactor.
    if (m_minCompCount == SizeClass::Other) {
      if (semTy->isArrayTy()) {
        semTy = semTy->getArrayElementType();
        // TessFactor or InsideTessFactor must either be float[2] or float
        if ((m_Kind == Kind::TessFactor ||
             m_Kind == Kind::InsideTessFactor) &&
          !dxilutil::IsIntegerOrFloatingPointType(semTy)) {
          return false;
        }
        // Clip/Cull can be array of scalar or vector
        if ((m_Kind == Kind::ClipDistance ||
          m_Kind == Kind::CullDistance) &&
          !IsScalarOrVectorTy(semTy)) {
          return false;
        }
      }
      else {
        // Do not support other types such as matrix.
        return false;
      }
    }
    else {
      return false;
    }
  }

  if (((unsigned)m_allowedTys & (unsigned)GetCompType(semTy)) == 0)
    return false;

  // Skip type-shape validation for semantics marked as Other
  if (m_minCompCount == SizeClass::Other)
    return true;

  SizeClass compSzClass = GetCompCount(semTy);
  return compSzClass >= m_minCompCount &&
    compSzClass <= m_maxCompCount;
}

typedef Semantic SP;
const Semantic Semantic::ms_SemanticTable[kNumSemanticRecords] = {
  // Kind                         Name
  SP(Kind::Arbitrary,             nullptr,                     CompTy::AnyTy,              SizeClass::Other,  SizeClass::Other),
  SP(Kind::VertexID,              "SV_VertexID",               CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::InstanceID,            "SV_InstanceID",             CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::Position,              "SV_Position",               CompTy::HalfOrFloatTy,      SizeClass::Vec4,   SizeClass::Vec4),
  SP(Kind::RenderTargetArrayIndex,"SV_RenderTargetArrayIndex", CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::ViewPortArrayIndex,    "SV_ViewportArrayIndex",     CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::ClipDistance,          "SV_ClipDistance",           CompTy::HalfOrFloatTy,      SizeClass::Other,  SizeClass::Other),
  SP(Kind::CullDistance,          "SV_CullDistance",           CompTy::HalfOrFloatTy,      SizeClass::Other,  SizeClass::Other),
  SP(Kind::OutputControlPointID,  "SV_OutputControlPointID",   CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::DomainLocation,        "SV_DomainLocation",         CompTy::FloatTy,            SizeClass::Scalar, SizeClass::Vec3),
  SP(Kind::PrimitiveID,           "SV_PrimitiveID",            CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::GSInstanceID,          "SV_GSInstanceID",           CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::SampleIndex,           "SV_SampleIndex",            CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::IsFrontFace,           "SV_IsFrontFace",            CompTy::BoolOrInt32Ty,      SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::Coverage,              "SV_Coverage",               CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::InnerCoverage,         "SV_InnerCoverage",          CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::Target,                "SV_Target",                 CompTy::AnyTy,              SizeClass::Scalar, SizeClass::Vec4),
  SP(Kind::Depth,                 "SV_Depth",                  CompTy::HalfOrFloatTy,      SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::DepthLessEqual,        "SV_DepthLessEqual",         CompTy::HalfOrFloatTy,      SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::DepthGreaterEqual,     "SV_DepthGreaterEqual",      CompTy::HalfOrFloatTy,      SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::StencilRef,            "SV_StencilRef",             CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::DispatchThreadID,      "SV_DispatchThreadID",       CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec3),
  SP(Kind::GroupID,               "SV_GroupID",                CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec3),
  SP(Kind::GroupIndex,            "SV_GroupIndex",             CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::GroupThreadID,         "SV_GroupThreadID",          CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec3),
  SP(Kind::TessFactor,            "SV_TessFactor",             CompTy::HalfOrFloatTy,      SizeClass::Other,  SizeClass::Other),
  SP(Kind::InsideTessFactor,      "SV_InsideTessFactor",       CompTy::HalfOrFloatTy,      SizeClass::Other,  SizeClass::Other),
  SP(Kind::ViewID,                "SV_ViewID",                 CompTy::Int32Ty,            SizeClass::Scalar, SizeClass::Scalar),
  SP(Kind::Barycentrics,          "SV_Barycentrics",           CompTy::HalfOrFloatTy,      SizeClass::Vec3,   SizeClass::Vec3),
  SP(Kind::ShadingRate,           "SV_ShadingRate",            CompTy::Int16Or32Ty,        SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::CullPrimitive,         "SV_CullPrimitive",          CompTy::BoolOrInt16Or32Ty,  SizeClass::Scalar, SizeClass::Vec1),
  SP(Kind::Invalid,               nullptr,                     CompTy::AnyTy,              SizeClass::Other,  SizeClass::Other),
};

} // namespace hlsl
