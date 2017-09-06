///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignatureElement.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Representation of HLSL signature element.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/ADT/StringRef.h"
#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilInterpolationMode.h"
#include "dxc/HLSL/DxilCompType.h"
#include "dxc/HLSL/DxilSignatureAllocator.h"
#include <string>
#include <vector>


namespace hlsl {

class ShaderModel;

/// Use this class to represent HLSL signature elements.
class DxilSignatureElement {
  friend class DxilSignature;

public:
  using Kind = DXIL::SigPointKind;

  static const unsigned kUndefinedID = UINT_MAX;

  DxilSignatureElement(Kind K);
  virtual ~DxilSignatureElement();

  void Initialize(llvm::StringRef Name, const CompType &ElementType, const InterpolationMode &InterpMode, 
                  unsigned Rows, unsigned Cols, 
                  int StartRow = Semantic::kUndefinedRow, int StartCol = Semantic::kUndefinedCol,
                  unsigned ID = kUndefinedID, const std::vector<unsigned> &IndexVector = std::vector<unsigned>());

  unsigned GetID() const;
  void SetID(unsigned ID);

  DXIL::ShaderKind GetShaderKind() const;

  DXIL::SigPointKind GetSigPointKind() const;
  void SetSigPointKind(DXIL::SigPointKind K);

  bool IsInput() const;
  bool IsOutput() const;
  bool IsPatchConstant() const;
  const char *GetName() const;
  unsigned GetRows() const;
  void SetRows(unsigned Rows);
  unsigned GetCols() const;
  void SetCols(unsigned Cols);
  const InterpolationMode *GetInterpolationMode() const;
  CompType GetCompType() const;
  unsigned GetOutputStream() const;
  void SetOutputStream(unsigned Stream);

  // Semantic properties.
  const Semantic *GetSemantic() const;
  void SetKind(Semantic::Kind kind);
  Semantic::Kind GetKind() const;
  bool IsArbitrary() const;
  bool IsDepth() const;
  bool IsDepthLE() const;
  bool IsDepthGE() const;
  bool IsAnyDepth() const;
  DXIL::SemanticInterpretationKind GetInterpretation() const;

  llvm::StringRef GetSemanticName() const;
  unsigned GetSemanticStartIndex() const;

  // Low-level properties.
  int GetStartRow() const;
  void SetStartRow(int StartRow);
  int GetStartCol() const;
  void SetStartCol(int Component);
  const std::vector<unsigned> &GetSemanticIndexVec() const;
  void SetSemanticIndexVec(const std::vector<unsigned> &Vec);
  void AppendSemanticIndex(unsigned SemIdx);
  void SetCompType(CompType CT);
  uint8_t GetColsAsMask() const;
  bool IsAllocated() const;
  unsigned GetDynIdxCompMask() const;
  void SetDynIdxCompMask(unsigned DynIdxCompMask);

protected:
  DXIL::SigPointKind m_sigPointKind;
  const Semantic *m_pSemantic;
  unsigned m_ID;
  std::string m_Name;
  llvm::StringRef m_SemanticName;
  unsigned m_SemanticStartIndex;
  CompType m_CompType;
  InterpolationMode m_InterpMode;
  std::vector<unsigned> m_SemanticIndex;
  unsigned m_Rows;
  unsigned m_Cols;
  int m_StartRow;
  int m_StartCol;
  unsigned m_OutputStream;
  unsigned m_DynIdxCompMask;
};

class DxilPackElement : public DxilSignatureAllocator::PackElement {
  DxilSignatureElement *m_pSE;
  bool m_bUseMinPrecision;

public:
  DxilPackElement(DxilSignatureElement *pSE, bool useMinPrecision) : m_pSE(pSE), m_bUseMinPrecision(useMinPrecision) {}
  __override ~DxilPackElement() {}
  __override uint32_t GetID() const { return m_pSE->GetID(); }
  __override DXIL::SemanticKind GetKind() const { return m_pSE->GetKind(); }
  __override DXIL::InterpolationMode GetInterpolationMode() const { return m_pSE->GetInterpolationMode()->GetKind(); }
  __override DXIL::SemanticInterpretationKind GetInterpretation() const { return m_pSE->GetInterpretation(); }
  __override DXIL::SignatureDataWidth GetDataBitWidth() const {
    uint8_t size = m_pSE->GetCompType().GetSizeInBits();
    // bool, min precision, or 32 bit types map to 32 bit size.
    if (size == 16) {
      return m_bUseMinPrecision ? DXIL::SignatureDataWidth::Bits32 : DXIL::SignatureDataWidth::Bits16;
    }
    else if (size == 1 || size == 32) {
      return DXIL::SignatureDataWidth::Bits32;
    }
    return DXIL::SignatureDataWidth::Undefined;
  }
  __override uint32_t GetRows() const { return m_pSE->GetRows(); }
  __override uint32_t GetCols() const { return m_pSE->GetCols(); }
  __override bool IsAllocated() const { return m_pSE->IsAllocated(); }
  __override uint32_t GetStartRow() const { return m_pSE->GetStartRow(); }
  __override uint32_t GetStartCol() const { return m_pSE->GetStartCol(); }

  __override void ClearLocation() {
    m_pSE->SetStartRow(-1);
    m_pSE->SetStartCol(-1);
  }
  __override void SetLocation(uint32_t Row, uint32_t Col) {
    m_pSE->SetStartRow(Row);
    m_pSE->SetStartCol(Col);
  }

  DxilSignatureElement *Get() { return m_pSE; }
};

} // namespace hlsl
