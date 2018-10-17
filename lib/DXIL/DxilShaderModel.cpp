///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilShaderModel.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <limits.h>

#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DXIL/DxilSemantic.h"
#include "dxc/Support/Global.h"


namespace hlsl {

ShaderModel::ShaderModel(Kind Kind, unsigned Major, unsigned Minor, const char *pszName,
                         unsigned NumInputRegs, unsigned NumOutputRegs,
                         bool bUAVs, bool bTypedUavs,  unsigned NumUAVRegs)
: m_Kind(Kind)
, m_Major(Major)
, m_Minor(Minor)
, m_pszName(pszName)
, m_NumInputRegs(NumInputRegs)
, m_NumOutputRegs(NumOutputRegs)
, m_bUAVs(bUAVs)
, m_bTypedUavs(bTypedUavs)
, m_NumUAVRegs(NumUAVRegs) {
}

bool ShaderModel::operator==(const ShaderModel &other) const {
    return m_Kind          == other.m_Kind
        && m_Major         == other.m_Major
        && m_Minor         == other.m_Minor
        && strcmp(m_pszName,  other.m_pszName) == 0
        && m_NumInputRegs  == other.m_NumInputRegs
        && m_NumOutputRegs == other.m_NumOutputRegs
        && m_bTypedUavs    == other.m_bTypedUavs
        && m_NumUAVRegs    == other.m_NumUAVRegs;
}

bool ShaderModel::IsValid() const {
  DXASSERT(IsPS() || IsVS() || IsGS() || IsHS() || IsDS() || IsCS() ||
               IsLib() || m_Kind == Kind::Invalid,
           "invalid shader model");
  return m_Kind != Kind::Invalid;
}

bool ShaderModel::IsValidForDxil() const {
  if (!IsValid())
    return false;
  switch (m_Major) {
    case 6: {
      switch (m_Minor) {
      case 0:
      case 1:
      case 2:
      case 3:
        return true;
      case kOfflineMinor:
        return m_Kind == Kind::Library;
      }
    }
    break;
  }
  return false;
}

bool ShaderModel::IsValidForModule() const {
  // Ray tracing shader model should only be used on functions in a lib
  return IsValid() && !IsRay();
}

const ShaderModel *ShaderModel::Get(unsigned Idx) {
  DXASSERT_NOMSG(Idx < kNumShaderModels - 1);
  if (Idx < kNumShaderModels - 1)
    return &ms_ShaderModels[Idx];
  else
    return GetInvalid();
}

const ShaderModel *ShaderModel::Get(Kind Kind, unsigned Major, unsigned Minor) {
  // Linear search. Replaced by binary search if necessary.
  for (unsigned i = 0; i < kNumShaderModels; i++) {
    const ShaderModel *pSM = &ms_ShaderModels[i];
    if (pSM->m_Kind == Kind && pSM->m_Major == Major && pSM->m_Minor == Minor)
      return pSM;
  }

  return GetInvalid();
}

const ShaderModel *ShaderModel::GetByName(const char *pszName) {
  // [ps|vs|gs|hs|ds|cs]_[major]_[minor]
  Kind kind;
  switch (pszName[0]) {
  case 'p':   kind = Kind::Pixel;     break;
  case 'v':   kind = Kind::Vertex;    break;
  case 'g':   kind = Kind::Geometry;  break;
  case 'h':   kind = Kind::Hull;      break;
  case 'd':   kind = Kind::Domain;    break;
  case 'c':   kind = Kind::Compute;   break;
  case 'l':   kind = Kind::Library;   break;
  default:    return GetInvalid();
  }
  unsigned Idx = 3;
  if (kind != Kind::Library) {
    if (pszName[1] != 's' || pszName[2] != '_')
      return GetInvalid();
  } else {
    if (pszName[1] != 'i' || pszName[2] != 'b' || pszName[3] != '_')
      return GetInvalid();
    Idx = 4;
  }

  unsigned Major;
  switch (pszName[Idx++]) {
  case '4': Major = 4;  break;
  case '5': Major = 5;  break;
  case '6': Major = 6;  break;
  default:  return GetInvalid();
  }
  if (pszName[Idx++] != '_')
    return GetInvalid();

  unsigned Minor;
  switch (pszName[Idx++]) {
    case '0': Minor = 0;  break;
    case '1': Minor = 1;  break;
    case '2':
      if (Major == 6) {
        Minor = 2;
        break;
      }
      else return GetInvalid();
    case '3':
      if (Major == 6) {
        Minor = 3;
        break;
      }
      else return GetInvalid();
    case 'x':
      if (kind == Kind::Library && Major == 6) {
        Minor = kOfflineMinor;
        break;
      }
      else return GetInvalid();
    default:  return GetInvalid();
  }
  if (pszName[Idx++] != 0)
    return GetInvalid();

  return Get(kind, Major, Minor);
}

void ShaderModel::GetDxilVersion(unsigned &DxilMajor, unsigned &DxilMinor) const {
  DXASSERT(IsValidForDxil(), "invalid shader model");
  DxilMajor = 1;
  switch (m_Minor) {
  case 0:
    DxilMinor = 0;
    break;
  case 1:
    DxilMinor = 1;
    break;
  case 2:
    DxilMinor = 2;
    break;
  case 3:
  case kOfflineMinor: // Always update this to highest dxil version
    DxilMinor = 3;
    break;
  default:
    DXASSERT(0, "IsValidForDxil() should have caught this.");
    break;
  }
}

void ShaderModel::GetMinValidatorVersion(unsigned &ValMajor, unsigned &ValMinor) const {
  DXASSERT(IsValidForDxil(), "invalid shader model");
  ValMajor = 1;
  switch (m_Minor) {
  case 0:
    ValMinor = 0;
    break;
  case 1:
    ValMinor = 1;
    break;
  case 2:
    ValMinor = 2;
    break;
  case 3:
    ValMinor = 3;
    break;
  case kOfflineMinor:
    ValMajor = 0;
    ValMinor = 0;
    break;
  default:
    DXASSERT(0, "IsValidForDxil() should have caught this.");
    break;
  }
}

static const char *ShaderModelKindNames[] = {
    "ps", "vs", "gs", "hs", "ds", "cs", "lib",
    "raygeneration", "intersection", "anyhit", "closesthit", "miss", "callable",
    "invalid",
};

const char * ShaderModel::GetKindName() const {
  return GetKindName(m_Kind);
}

const char * ShaderModel::GetKindName(Kind kind) {
  return ShaderModelKindNames[static_cast<unsigned int>(kind)];
}

const ShaderModel *ShaderModel::GetInvalid() {
  return &ms_ShaderModels[kNumShaderModels - 1];
}

typedef ShaderModel SM;
typedef Semantic SE;
const ShaderModel ShaderModel::ms_ShaderModels[kNumShaderModels] = {
  //                                  IR  OR   UAV?   TyUAV? UAV base
  SM(Kind::Compute,  4, 0, "cs_4_0",  0,  0,   true,  false, 1),
  SM(Kind::Compute,  4, 1, "cs_4_1",  0,  0,   true,  false, 1),
  SM(Kind::Compute,  5, 0, "cs_5_0",  0,  0,   true,  true,  64),
  SM(Kind::Compute,  5, 1, "cs_5_1",  0,  0,   true,  true,  UINT_MAX),
  SM(Kind::Compute,  6, 0, "cs_6_0",  0,  0,   true,  true,  UINT_MAX),
  SM(Kind::Compute,  6, 1, "cs_6_1",  0,  0,   true,  true,  UINT_MAX),
  SM(Kind::Compute,  6, 2, "cs_6_2",  0,  0,   true,  true,  UINT_MAX),
  SM(Kind::Compute,  6, 3, "cs_6_3",  0,  0,   true,  true,  UINT_MAX),

  SM(Kind::Domain,   5, 0, "ds_5_0",  32, 32,  true,  true,  64),
  SM(Kind::Domain,   5, 1, "ds_5_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Domain,   6, 0, "ds_6_0",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Domain,   6, 1, "ds_6_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Domain,   6, 2, "ds_6_2",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Domain,   6, 3, "ds_6_3",  32, 32,  true,  true,  UINT_MAX),

  SM(Kind::Geometry, 4, 0, "gs_4_0",  16, 32,  false, false, 0),
  SM(Kind::Geometry, 4, 1, "gs_4_1",  32, 32,  false, false, 0),
  SM(Kind::Geometry, 5, 0, "gs_5_0",  32, 32,  true,  true,  64),
  SM(Kind::Geometry, 5, 1, "gs_5_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Geometry, 6, 0, "gs_6_0",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Geometry, 6, 1, "gs_6_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Geometry, 6, 2, "gs_6_2",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Geometry, 6, 3, "gs_6_3",  32, 32,  true,  true,  UINT_MAX),

  SM(Kind::Hull,     5, 0, "hs_5_0",  32, 32,  true,  true,  64),
  SM(Kind::Hull,     5, 1, "hs_5_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Hull,     6, 0, "hs_6_0",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Hull,     6, 1, "hs_6_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Hull,     6, 2, "hs_6_2",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Hull,     6, 3, "hs_6_3",  32, 32,  true,  true,  UINT_MAX),

  SM(Kind::Pixel,    4, 0, "ps_4_0",  32, 8,   false, false, 0),
  SM(Kind::Pixel,    4, 1, "ps_4_1",  32, 8,   false, false, 0),
  SM(Kind::Pixel,    5, 0, "ps_5_0",  32, 8,   true,  true,  64),
  SM(Kind::Pixel,    5, 1, "ps_5_1",  32, 8,   true,  true,  UINT_MAX),
  SM(Kind::Pixel,    6, 0, "ps_6_0",  32, 8,   true,  true,  UINT_MAX),
  SM(Kind::Pixel,    6, 1, "ps_6_1",  32, 8,   true,  true,  UINT_MAX),
  SM(Kind::Pixel,    6, 2, "ps_6_2",  32, 8,   true,  true,  UINT_MAX),
  SM(Kind::Pixel,    6, 3, "ps_6_3",  32, 8,   true,  true,  UINT_MAX),

  SM(Kind::Vertex,   4, 0, "vs_4_0",  16, 16,  false, false, 0),
  SM(Kind::Vertex,   4, 1, "vs_4_1",  32, 32,  false, false, 0),
  SM(Kind::Vertex,   5, 0, "vs_5_0",  32, 32,  true,  true,  64),
  SM(Kind::Vertex,   5, 1, "vs_5_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Vertex,   6, 0, "vs_6_0",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Vertex,   6, 1, "vs_6_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Vertex,   6, 2, "vs_6_2",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Vertex,   6, 3, "vs_6_3",  32, 32,  true,  true,  UINT_MAX),

  SM(Kind::Library,  6, 1, "lib_6_1",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Library,  6, 2, "lib_6_2",  32, 32,  true,  true,  UINT_MAX),
  SM(Kind::Library,  6, 3, "lib_6_3",  32, 32,  true,  true,  UINT_MAX),

  // lib_6_x is for offline linking only, and relaxes restrictions
  SM(Kind::Library,  6, kOfflineMinor, "lib_6_x",  32, 32,  true,  true,  UINT_MAX),

  SM(Kind::Invalid,  0, 0, "invalid", 0,  0,   false, false, 0),
};

} // namespace hlsl
