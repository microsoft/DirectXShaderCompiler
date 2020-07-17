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
                   CompClass compClass,
                   SizeClass sizeClass)
: m_Kind(Kind)
, m_pszName(pszName)
, m_CompClass(compClass)
, m_SizeClass(sizeClass)
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

typedef Semantic SP;
const Semantic Semantic::ms_SemanticTable[kNumSemanticRecords] = {
  // Kind                         Name                        Component         Size
  SP(Kind::Arbitrary,             nullptr,                    CompClass::Any,   SizeClass::Other),
  SP(Kind::VertexID,              "SV_VertexID",              CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::InstanceID,            "SV_InstanceID",            CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::Position,              "SV_Position",              CompClass::Float, SizeClass::Vec4),
  SP(Kind::RenderTargetArrayIndex,"SV_RenderTargetArrayIndex",CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::ViewPortArrayIndex,    "SV_ViewportArrayIndex",    CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::ClipDistance,          "SV_ClipDistance",          CompClass::Float, SizeClass::Other),
  SP(Kind::CullDistance,          "SV_CullDistance",          CompClass::Float, SizeClass::Other),
  SP(Kind::OutputControlPointID,  "SV_OutputControlPointID",  CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::DomainLocation,        "SV_DomainLocation",        CompClass::Float, SizeClass::MaxVec3),
  SP(Kind::PrimitiveID,           "SV_PrimitiveID",           CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::GSInstanceID,          "SV_GSInstanceID",          CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::SampleIndex,           "SV_SampleIndex",           CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::IsFrontFace,           "SV_IsFrontFace",           CompClass::Bool,  SizeClass::Scalar),
  SP(Kind::Coverage,              "SV_Coverage",              CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::InnerCoverage,         "SV_InnerCoverage",         CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::Target,                "SV_Target",                CompClass::Any,   SizeClass::MaxVec4),
  SP(Kind::Depth,                 "SV_Depth",                 CompClass::Float, SizeClass::Scalar),
  SP(Kind::DepthLessEqual,        "SV_DepthLessEqual",        CompClass::Float, SizeClass::Scalar),
  SP(Kind::DepthGreaterEqual,     "SV_DepthGreaterEqual",     CompClass::Float, SizeClass::Scalar),
  SP(Kind::StencilRef,            "SV_StencilRef",            CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::DispatchThreadID,      "SV_DispatchThreadID",      CompClass::Uint,  SizeClass::MaxVec3),
  SP(Kind::GroupID,               "SV_GroupID",               CompClass::Uint,  SizeClass::MaxVec3),
  SP(Kind::GroupIndex,            "SV_GroupIndex",            CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::GroupThreadID,         "SV_GroupThreadID",         CompClass::Uint,  SizeClass::MaxVec3),
  SP(Kind::TessFactor,            "SV_TessFactor",            CompClass::Float, SizeClass::Other),
  SP(Kind::InsideTessFactor,      "SV_InsideTessFactor",      CompClass::Float, SizeClass::Other),
  SP(Kind::ViewID,                "SV_ViewID",                CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::Barycentrics,          "SV_Barycentrics",          CompClass::Float, SizeClass::MaxVec3),
  SP(Kind::ShadingRate,           "SV_ShadingRate",           CompClass::Uint,  SizeClass::Scalar),
  SP(Kind::CullPrimitive,         "SV_CullPrimitive",         CompClass::Bool,  SizeClass::Scalar),
  SP(Kind::Invalid,               nullptr,                    CompClass::Any,   SizeClass::Other),
};

} // namespace hlsl
