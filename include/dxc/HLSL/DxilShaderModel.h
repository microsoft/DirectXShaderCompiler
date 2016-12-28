///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilShaderModel.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL shader models.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DXILConstants.h"
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
  static const unsigned kHighestMinor = 0;

  bool IsPS() const     { return m_Kind == Kind::Pixel; }
  bool IsVS() const     { return m_Kind == Kind::Vertex; }
  bool IsGS() const     { return m_Kind == Kind::Geometry; }
  bool IsHS() const     { return m_Kind == Kind::Hull; }
  bool IsDS() const     { return m_Kind == Kind::Domain; }
  bool IsCS() const     { return m_Kind == Kind::Compute; }
  bool IsValid() const;

  Kind GetKind() const      { return m_Kind; }
  unsigned GetMajor() const { return m_Major; }
  unsigned GetMinor() const { return m_Minor; }
  bool IsSM50Plus() const   { return m_Major >= 5; }
  bool IsSM51Plus() const   { return m_Major > 5 || (m_Major == 5 && m_Minor >= 1); }
  const char *GetName() const { return m_pszName; }
  std::string GetKindName() const;
  unsigned GetNumTempRegs() const { return DXIL::kMaxTempRegCount; }
  unsigned GetNumInputRegs() const { return m_NumInputRegs; }
  unsigned GetNumOutputRegs() const { return m_NumOutputRegs; }
  unsigned GetNumResources() const;
  unsigned GetNumSamplers() const;
  unsigned GetNumCBuffers() const;
  unsigned GetCBufferSize() const { return DXIL::kMaxCBufferSize; }
  unsigned SupportsUAV() const { return m_bUAVs; }
  unsigned SupportsTypedUAVs() const { return m_bTypedUavs; }
  unsigned GetUAVRegLimit() const { return m_NumUAVRegs; }
  unsigned GetUAVRegsBase() const;

  static unsigned Count() { return kNumShaderModels - 1; }
  static const ShaderModel *Get(unsigned Idx);
  static const ShaderModel *Get(Kind Kind, unsigned Major, unsigned Minor);
  static const ShaderModel *GetByName(const char *pszName);

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

  static const unsigned kNumShaderModels = 27;
  static const ShaderModel ms_ShaderModels[kNumShaderModels];

  static const ShaderModel *GetInvalid();
};

} // namespace hlsl
