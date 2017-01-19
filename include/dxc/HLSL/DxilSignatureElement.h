///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignatureElement.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL signature element.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/ADT/StringRef.h"
#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilInterpolationMode.h"
#include "dxc/HLSL/DxilCompType.h"
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
};

} // namespace hlsl
