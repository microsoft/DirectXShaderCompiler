///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignatureElement.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Representation of HLSL signature element.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilContainer.h"
#include <memory>

using std::string;
using std::vector;
using std::unique_ptr;


namespace hlsl {

//------------------------------------------------------------------------------
//
// DxilSignatureElement methods.
//
DxilSignatureElement::DxilSignatureElement(DXIL::SigPointKind sigPointKind)
: m_sigPointKind(sigPointKind)
, m_pSemantic(nullptr)
, m_ID(kUndefinedID)
, m_CompType(CompType::Kind::Invalid)
, m_InterpMode(InterpolationMode::Kind::Invalid)
, m_Rows(0)
, m_Cols(0)
, m_StartRow(Semantic::kUndefinedRow)
, m_StartCol(Semantic::kUndefinedCol)
, m_DynIdxCompMask(0) {
}

DxilSignatureElement::~DxilSignatureElement() {
}

void DxilSignatureElement::Initialize(llvm::StringRef Name, const CompType &ElementType, 
                                      const InterpolationMode &InterpMode, 
                                      unsigned Rows, unsigned Cols, 
                                      int StartRow, int StartCol,
                                      unsigned ID, const vector<unsigned> &IndexVector) {
  DXASSERT(m_pSemantic == nullptr, "an instance should be initiazed only once");

  m_ID = ID;
  m_Name = Name.str(); // creates a copy
  Semantic::DecomposeNameAndIndex(m_Name, &m_SemanticName, &m_SemanticStartIndex);
  if (!IndexVector.empty())
    m_SemanticStartIndex = IndexVector[0];
  // Find semantic in the table.
  m_pSemantic = Semantic::GetByName(m_SemanticName, m_sigPointKind);
  m_CompType = ElementType;
  m_InterpMode = InterpMode;
  m_SemanticIndex = IndexVector;
  m_Rows = Rows;
  m_Cols = Cols;
  m_StartRow = StartRow;
  m_StartCol = StartCol;
  m_OutputStream = 0;
}

unsigned DxilSignatureElement::GetID() const {
  return m_ID;
}

void DxilSignatureElement::SetID(unsigned ID) {
  DXASSERT_NOMSG(m_ID == kUndefinedID || m_ID == ID);
  m_ID = ID;
}

DXIL::ShaderKind DxilSignatureElement::GetShaderKind() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->GetShaderKind();
}

bool DxilSignatureElement::IsInput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsInput();
}

bool DxilSignatureElement::IsOutput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsOutput();
}

bool DxilSignatureElement::IsPatchConstant() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsPatchConstant();
}

const char *DxilSignatureElement::GetName() const {
  if (IsArbitrary())
    return m_Name.c_str();
  else if (!m_pSemantic->IsInvalid())
    return m_pSemantic->GetName();
  else
    return m_Name.c_str();
}

unsigned DxilSignatureElement::GetRows() const {
  return m_Rows;
}
void DxilSignatureElement::SetRows(unsigned Rows) {
  m_Rows = Rows;
}

unsigned DxilSignatureElement::GetCols() const {
  return m_Cols;
}

void DxilSignatureElement::SetCols(unsigned Cols) {
  m_Cols = Cols;
}

const InterpolationMode *DxilSignatureElement::GetInterpolationMode() const {
  return &m_InterpMode;
}

CompType DxilSignatureElement::GetCompType() const {
  return m_CompType;
}

unsigned DxilSignatureElement::GetOutputStream() const {
  return m_OutputStream;
}

void DxilSignatureElement::SetOutputStream(unsigned Stream) {
  m_OutputStream = Stream;
}

DXIL::SigPointKind DxilSignatureElement::GetSigPointKind() const {
  return m_sigPointKind;
}

void DxilSignatureElement::SetSigPointKind(DXIL::SigPointKind K) {
  m_sigPointKind = K;
}


//
// Semantic-related properties.
//
const Semantic *DxilSignatureElement::GetSemantic() const {
  return m_pSemantic;
}

void DxilSignatureElement::SetKind(Semantic::Kind kind) {
  // recover the original SigPointKind if necessary (for Shadow element).
  m_sigPointKind = SigPoint::RecoverKind(kind, m_sigPointKind);
  m_pSemantic = Semantic::Get(kind, m_sigPointKind);
}

Semantic::Kind DxilSignatureElement::GetKind() const {
  return m_pSemantic->GetKind();
}

bool DxilSignatureElement::IsArbitrary() const {
  return m_pSemantic->IsArbitrary();
}

bool DxilSignatureElement::IsDepth() const {
  return m_pSemantic->GetKind() == Semantic::Kind::Depth;
}

bool DxilSignatureElement::IsDepthLE() const {
  return m_pSemantic->GetKind() == Semantic::Kind::DepthLessEqual;
}

bool DxilSignatureElement::IsDepthGE() const {
  return m_pSemantic->GetKind() == Semantic::Kind::DepthGreaterEqual;
}

bool DxilSignatureElement::IsAnyDepth() const {
  return IsDepth() || IsDepthLE() || IsDepthGE();
}

DXIL::SemanticInterpretationKind DxilSignatureElement::GetInterpretation() const {
  return SigPoint::GetInterpretation(m_pSemantic->GetKind(), m_sigPointKind, ShaderModel::kHighestMajor, ShaderModel::kHighestMinor);
}

llvm::StringRef DxilSignatureElement::GetSemanticName() const {
  return m_SemanticName;
}
unsigned DxilSignatureElement::GetSemanticStartIndex() const {
  return m_SemanticStartIndex;
}

//
// Low-level properties.
//
int DxilSignatureElement::GetStartRow() const {
  return m_StartRow;
}

void DxilSignatureElement::SetStartRow(int StartRow) {
  m_StartRow = StartRow;
}

int DxilSignatureElement::GetStartCol() const {
  return m_StartCol;
}

void DxilSignatureElement::SetStartCol(int StartCol) {
  m_StartCol = StartCol;
}

const std::vector<unsigned> &DxilSignatureElement::GetSemanticIndexVec() const {
  return m_SemanticIndex;
}

void DxilSignatureElement::SetSemanticIndexVec(const std::vector<unsigned> &Vec) {
  m_SemanticIndex = Vec;
}

void DxilSignatureElement::AppendSemanticIndex(unsigned SemIdx) {
  m_SemanticIndex.emplace_back(SemIdx);
}

void DxilSignatureElement::SetCompType(CompType CT) {
  m_CompType = CT;
}

uint8_t DxilSignatureElement::GetColsAsMask() const {
  unsigned StartCol = IsAllocated() ? m_StartCol : 0;
  DXASSERT(StartCol + m_Cols <= 4, "else start %u and cols %u exceed limit", StartCol, m_Cols);
  DXASSERT(m_Cols >= 1, "else signature takes no space");
  switch (StartCol) {
  case 0: {
    switch (m_Cols) {
    case 1:
      return DxilProgramSigMaskX;
    case 2:
      return DxilProgramSigMaskX | DxilProgramSigMaskY;
    case 3:
      return DxilProgramSigMaskX | DxilProgramSigMaskY | DxilProgramSigMaskZ;
    default:
      return DxilProgramSigMaskX | DxilProgramSigMaskY | DxilProgramSigMaskZ | DxilProgramSigMaskW;
    }
  }
  case 1: {
    switch (m_Cols) {
    case 1:
      return DxilProgramSigMaskY;
    case 2:
      return DxilProgramSigMaskY | DxilProgramSigMaskZ;
    default:
      return DxilProgramSigMaskY | DxilProgramSigMaskZ | DxilProgramSigMaskW;
    }
  }
  case 2: return DxilProgramSigMaskZ | ((m_Cols == 1) ? 0 : DxilProgramSigMaskW);
  case 3:
  default:
    return DxilProgramSigMaskW;
  }
}

bool DxilSignatureElement::IsAllocated() const {
  return (m_StartRow != Semantic::kUndefinedRow) && (m_StartCol != Semantic::kUndefinedCol);
}

unsigned DxilSignatureElement::GetDynIdxCompMask() const {
  DXASSERT_NOMSG(m_DynIdxCompMask <= 0xF);
  return m_DynIdxCompMask;
}

void DxilSignatureElement::SetDynIdxCompMask(unsigned DynIdxCompMask) {
  DXASSERT_NOMSG(DynIdxCompMask <= 0xF);
  m_DynIdxCompMask = DynIdxCompMask;
}

} // namespace hlsl
