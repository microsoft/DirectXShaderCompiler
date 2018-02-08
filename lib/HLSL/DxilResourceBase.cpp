///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilResourceBase.cpp                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilResourceBase.h"
#include "dxc/Support/Global.h"


namespace hlsl {


//------------------------------------------------------------------------------
//
// ResourceBase methods.
//
DxilResourceBase::DxilResourceBase(Class C)
: m_Class(C)
, m_Kind(Kind::Invalid)
, m_ID(UINT_MAX)
, m_SpaceID(0)
, m_LowerBound(0)
, m_RangeSize(0) 
, m_pSymbol(nullptr) 
, m_pHandle(nullptr) {
}

DxilResourceBase::Class DxilResourceBase::GetClass() const { return m_Class; }
DxilResourceBase::Kind DxilResourceBase::GetKind() const { return m_Kind; }
void DxilResourceBase::SetKind(DxilResourceBase::Kind ResourceKind) {
  DXASSERT(ResourceKind > Kind::Invalid && ResourceKind < Kind::NumEntries, "otherwise the caller passed wrong resource type");
  m_Kind = ResourceKind;
}

unsigned DxilResourceBase::GetID() const          { return m_ID; }
unsigned DxilResourceBase::GetSpaceID() const     { return m_SpaceID; }
unsigned DxilResourceBase::GetLowerBound() const  { return m_LowerBound; }
unsigned DxilResourceBase::GetUpperBound() const  { return m_RangeSize != UINT_MAX ? m_LowerBound + m_RangeSize - 1 : UINT_MAX; }
unsigned DxilResourceBase::GetRangeSize() const   { return m_RangeSize; }
llvm::Constant *DxilResourceBase::GetGlobalSymbol() const { return m_pSymbol; }
const std::string &DxilResourceBase::GetGlobalName() const      { return m_Name; }
llvm::Value *DxilResourceBase::GetHandle() const  { return m_pHandle; }
bool DxilResourceBase::IsAllocated() const        { return m_LowerBound != UINT_MAX; }
bool DxilResourceBase::IsUnbounded() const        { return m_RangeSize == UINT_MAX; }

void DxilResourceBase::SetClass(Class C)                          { m_Class = C; }
void DxilResourceBase::SetID(unsigned ID)                         { m_ID = ID; }
void DxilResourceBase::SetSpaceID(unsigned SpaceID)               { m_SpaceID = SpaceID; }
void DxilResourceBase::SetLowerBound(unsigned LB)                 { m_LowerBound = LB; }
void DxilResourceBase::SetRangeSize(unsigned RangeSize)           { m_RangeSize = RangeSize; }
void DxilResourceBase::SetGlobalSymbol(llvm::Constant *pGV)       { m_pSymbol = pGV; }
void DxilResourceBase::SetGlobalName(const std::string &Name)     { m_Name = Name; }
void DxilResourceBase::SetHandle(llvm::Value *pHandle)            { m_pHandle = pHandle; }

static const char *s_ResourceClassNames[(unsigned)DxilResourceBase::Class::Invalid] = {
    "texture", "UAV", "cbuffer", "sampler"
};

const char *DxilResourceBase::GetResClassName() const {
  return s_ResourceClassNames[(unsigned)m_Class];
}

static const char *s_ResourceIDPrefixs[(unsigned)DxilResourceBase::Class::Invalid] = {
    "T", "U", "CB", "S"
};

const char *DxilResourceBase::GetResIDPrefix() const {
  return s_ResourceIDPrefixs[(unsigned)m_Class];
}

static const char *s_ResourceBindPrefixs[(unsigned)DxilResourceBase::Class::Invalid] = {
    "t", "u", "cb", "s"
};

const char *DxilResourceBase::GetResBindPrefix() const {
  return s_ResourceBindPrefixs[(unsigned)m_Class];
}

static const char *s_ResourceDimNames[(unsigned)DxilResourceBase::Kind::NumEntries] = {
        "invalid", "1d",        "2d",      "2dMS",      "3d",
        "cube",    "1darray",   "2darray", "2darrayMS", "cubearray",
        "buf",     "rawbuf",    "structbuf", "cbuffer", "sampler",
        "tbuffer", "ras",
};

const char *DxilResourceBase::GetResDimName() const {
  return s_ResourceDimNames[(unsigned)m_Kind];
}

} // namespace hlsl
