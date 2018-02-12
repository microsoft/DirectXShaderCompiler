///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilShaderModel.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Representation of HLSL shader models.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilConstants.h"
#include <string>


namespace hlsl {

class Semantic;


/// <summary>
/// Use this class to represent HLSL shader model.
/// </summary>
class ShaderModel {
public:
  using Kind = DXIL::ShaderKind;

  // Major/Minor version of highest shader model
  static const unsigned kHighestMajor = 6;
  static const unsigned kHighestMinor = 3;

  bool IsPS() const     { return m_Kind == Kind::Pixel; }
  bool IsVS() const     { return m_Kind == Kind::Vertex; }
  bool IsGS() const     { return m_Kind == Kind::Geometry; }
  bool IsHS() const     { return m_Kind == Kind::Hull; }
  bool IsDS() const     { return m_Kind == Kind::Domain; }
  bool IsCS() const     { return m_Kind == Kind::Compute; }
  bool IsLib() const    { return m_Kind == Kind::Library; }
  bool IsValid() const;
  bool IsValidForDxil() const;

  Kind GetKind() const      { return m_Kind; }
  unsigned GetMajor() const { return m_Major; }
  unsigned GetMinor() const { return m_Minor; }
  void GetDxilVersion(unsigned &DxilMajor, unsigned &DxilMinor) const;
  void GetMinValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const;
  bool IsSM50Plus() const   { return m_Major >= 5; }
  bool IsSM51Plus() const   { return m_Major > 5 || (m_Major == 5 && m_Minor >= 1); }
  bool IsSM60Plus() const   { return m_Major >= 6; }
  bool IsSM61Plus() const   { return m_Major > 6 || (m_Major == 6 && m_Minor >= 1); }
  const char *GetName() const { return m_pszName; }
  std::string GetKindName() const;
  unsigned GetNumTempRegs() const { return DXIL::kMaxTempRegCount; }
  unsigned GetNumInputRegs() const { return m_NumInputRegs; }
  unsigned GetNumOutputRegs() const { return m_NumOutputRegs; }
  unsigned GetCBufferSize() const { return DXIL::kMaxCBufferSize; }
  unsigned SupportsUAV() const { return m_bUAVs; }
  unsigned SupportsTypedUAVs() const { return m_bTypedUavs; }
  unsigned GetUAVRegLimit() const { return m_NumUAVRegs; }
  DXIL::PackingStrategy GetDefaultPackingStrategy() const { return DXIL::PackingStrategy::PrefixStable; }

  static unsigned Count() { return kNumShaderModels - 1; }
  static const ShaderModel *Get(unsigned Idx);
  static const ShaderModel *Get(Kind Kind, unsigned Major, unsigned Minor);
  static const ShaderModel *GetByName(const char *pszName);
  static std::string GetKindName(Kind kind);

  bool operator==(const ShaderModel &other) const;
  bool operator!=(const ShaderModel &other) const { return !(*this == other); }

private:
  Kind m_Kind;
  unsigned m_Major;
  unsigned m_Minor;
  const char *m_pszName;
  unsigned m_NumInputRegs;
  unsigned m_NumOutputRegs;
  bool     m_bUAVs;
  bool     m_bTypedUavs;
  unsigned m_NumUAVRegs;

  ShaderModel() = delete;
  ShaderModel(Kind Kind, unsigned Major, unsigned Minor, const char *pszName,
              unsigned m_NumInputRegs, unsigned m_NumOutputRegs,
              bool m_bUAVs, bool m_bTypedUavs, unsigned m_UAVRegsLim);

  static const unsigned kNumShaderModels = 48;
  static const ShaderModel ms_ShaderModels[kNumShaderModels];

  static const ShaderModel *GetInvalid();
};

} // namespace hlsl
